# verify.ps1 - one-shot verification of the working tree.
#
# Runs the whole ladder below the emulator in one command and prints a
# pass/fail line per rung, so a change can be checked without forming a
# judgement:
#
#   1. Psion build via DOSBox, errors and warnings read back from build.log
#   2. PSION3D.IMG hashed against the build before this one
#      (unchanged = refactor-safe, changed = expected for a feature)
#   3. DGROUP check via tools\memcheck.ps1, with the previous map as baseline
#      so any region that grew is named
#   4. PC host build via CMake (configured on first run)
#   5. Golden frames: every view in golden\views.txt rendered headlessly and
#      compared pixel by pixel with golden\<name>.png
#
#   .\tools\verify.bat                 everything
#   .\tools\verify.bat -SkipPsion      PC build and frames only (no DOSBox)
#   .\tools\verify.bat -SkipPc         Psion build, hash and DGROUP only
#   .\tools\verify.bat -UpdateGolden   accept the rendered frames as the new goldens
#   .\tools\verify.bat -Limit 50000    DGROUP limit passed through to memcheck
#
# Exit codes: 0 all rungs passed, 1 something failed, 3 another verify is
# already running (the DOSBox mount is pinned to this directory, so builds
# cannot overlap; this fails fast rather than queueing).
#
# Working files go in .verify\ (gitignored): the previous map for the DGROUP
# baseline and the rendered frames.

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$tools = Join-Path $root "tools"
$work = Join-Path $root ".verify"

# Toolchain locations. pc\README.md explains why these are spelled out: the
# cmake on PATH is devkitPro's and MinGW must be found ahead of MSYS2.
$DosBox = "C:\Program Files (x86)\DOSBox-0.74-3\DOSBox.exe"
$CMake = "C:\Program Files\CMake\bin\cmake.exe"
$MinGwBin = "C:\Qt\Tools\mingw1310_64\bin"
$QtDir = "C:\Qt\6.9.1\mingw_64"

$SkipPsion = $false
$SkipPc = $false
$UpdateGolden = $false
$Limit = $null

for($argIndex = 0; $argIndex -lt $args.Count; $argIndex++)
{
	$arg = $args[$argIndex]

	if($arg -eq "-SkipPsion") { $SkipPsion = $true; continue }
	if($arg -eq "-SkipPc") { $SkipPc = $true; continue }
	if($arg -eq "-UpdateGolden") { $UpdateGolden = $true; continue }

	if($arg -eq "-Limit")
	{
		$argIndex++

		if($argIndex -ge $args.Count)
		{
			throw "-Limit requires a value."
		}

		$Limit = $args[$argIndex]
		continue
	}

	throw "Unknown argument: $arg"
}

# --- reporting ---------------------------------------------------------------

$results = @()

function Report($status, $step, $detail)
{
	$script:results += @{ Status = $status; Step = $step; Detail = $detail }
	Write-Output ('[{0,-4}] {1,-14} {2}' -f $status, $step, $detail)
}

function Indent($text)
{
	foreach($line in ($text -split "\r?\n"))
	{
		if($line.Trim() -ne "")
		{
			Write-Output "        $line"
		}
	}
}

# --- lock --------------------------------------------------------------------

# One verify at a time per machine: DOSBox's D: mount is this directory.
$lockPath = Join-Path $env:TEMP "psion3d-verify.lock"
$lock = $null

try
{
	$lock = [IO.File]::Open($lockPath, [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
}
catch
{
	Write-Output "verify: another verify is already running (lock: $lockPath)"
	exit 3
}

try
{
	if(-not (Test-Path $work))
	{
		New-Item -ItemType Directory -Path $work | Out-Null
	}

	$imgPath = Join-Path $root "PSION3D.IMG"
	$exePath = Join-Path $root "PSION3D.EXE"
	$mapPath = Join-Path $root "PSION3D.MAP"
	$logPath = Join-Path $root "build.log"
	$prevMap = Join-Path $work "prev.MAP"

	# --- 1. Psion build ------------------------------------------------------

	$psionOk = $true

	if($SkipPsion)
	{
		Report "SKIP" "psion build" "-SkipPsion"
	}
	else
	{
		if(-not (Test-Path $DosBox))
		{
			Report "FAIL" "psion build" "DOSBox not found at $DosBox"
			$psionOk = $false
		}
		else
		{
			# build.log open elsewhere means a build is still writing it.
			if(Test-Path $logPath)
			{
				try
				{
					$probe = [IO.File]::Open($logPath, [IO.FileMode]::Open, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
					$probe.Close()
				}
				catch
				{
					Write-Output "verify: build.log is locked - a build is already running"
					exit 3
				}
			}

			$prevHash = $null

			if(Test-Path $imgPath)
			{
				$prevHash = (Get-FileHash $imgPath).Hash
			}

			if(Test-Path $mapPath)
			{
				Copy-Item $mapPath $prevMap -Force
			}

			$sw = [Diagnostics.Stopwatch]::StartNew()
			& $DosBox -c "D:" -c "tsc /m unnamed.pr /smain=psion3d /v0 /zq > D:\build.log" -c "exit" | Out-Null
			$seconds = [math]::Round($sw.Elapsed.TotalSeconds, 1)

			$log = ""

			if(Test-Path $logPath)
			{
				$log = [IO.File]::ReadAllText($logPath)
			}

			$errors = @($log -split "\r?\n" | Where-Object { $_ -match 'Error' })
			$warnings = @($log -split "\r?\n" | Where-Object { $_ -match 'Warning' })

			if($errors.Count -gt 0 -or -not (Test-Path $exePath))
			{
				$psionOk = $false
				Report "FAIL" "psion build" "$($errors.Count) error(s) in build.log, ${seconds}s"
				Indent ($errors -join "`n")

				if($errors.Count -eq 0)
				{
					Indent "PSION3D.EXE is missing and build.log has no Error line; the log is:"
					Indent $log
				}
			}
			else
			{
				$note = "${seconds}s"

				if($warnings.Count -gt 0)
				{
					$note += ", $($warnings.Count) warning(s)"
				}

				Report "OK" "psion build" $note

				if($warnings.Count -gt 0)
				{
					Indent ($warnings -join "`n")
				}
			}

			# --- 2. image hash -----------------------------------------------

			if($psionOk)
			{
				$hash = (Get-FileHash $imgPath).Hash
				$short = $hash.Substring(0, 12).ToLower()

				if($prevHash -eq $null)
				{
					Report "OK" "image" "$short (no previous image to compare)"
				}
				elseif($hash -eq $prevHash)
				{
					Report "OK" "image" "$short unchanged - refactor-safe"
				}
				else
				{
					Report "OK" "image" "$short CHANGED from $($prevHash.Substring(0, 12).ToLower()) - expected for a feature, not for a refactor"
				}
			}
		}
	}

	# --- 3. DGROUP -------------------------------------------------------------

	if(-not $psionOk)
	{
		Report "SKIP" "dgroup" "no build"
	}
	elseif(-not (Test-Path $mapPath))
	{
		Report "FAIL" "dgroup" "PSION3D.MAP missing"
	}
	else
	{
		$memArgs = @()

		if((Test-Path $prevMap) -and -not $SkipPsion)
		{
			$memArgs += @("-Baseline", $prevMap)
		}

		if($Limit -ne $null)
		{
			$memArgs += @("-Limit", $Limit)
		}

		$memOut = & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $tools "memcheck.ps1") @memArgs 2>&1 | Out-String
		$memExit = $LASTEXITCODE

		$dgroupLine = ($memOut -split "\r?\n" | Where-Object { $_ -match 'Near data' } | Select-Object -First 1)
		$summary = ""

		if($dgroupLine -ne $null)
		{
			$summary = ($dgroupLine -replace '^Near data \(DGROUP\)\s+', '') -replace '\s+', ' '
		}

		if($memExit -eq 0)
		{
			Report "OK" "dgroup" $summary
		}
		else
		{
			Report "FAIL" "dgroup" "memcheck exit $memExit $summary"
		}

		# The per-segment and per-region breakdown is the useful part; show it
		# whenever something moved, or always on failure.
		if($memExit -ne 0 -or $memOut -match 'regions that changed' -or $memOut -match '\+\d' -or $memOut -match ' -\d')
		{
			Indent $memOut
		}
	}

	# --- 4. PC build -----------------------------------------------------------

	$pcOk = $true
	$pcExe = Join-Path $root "pc\build\psion3d_pc.exe"

	if($SkipPc)
	{
		Report "SKIP" "pc build" "-SkipPc"
		$pcOk = Test-Path $pcExe
	}
	else
	{
		$env:PATH = "$MinGwBin;" + (Split-Path -Parent $CMake) + ";$env:PATH"
		$buildDir = Join-Path $root "pc\build"
		$sw = [Diagnostics.Stopwatch]::StartNew()
		$configOut = ""

		if(-not (Test-Path (Join-Path $buildDir "CMakeCache.txt")))
		{
			$configOut = & $CMake -S (Join-Path $root "pc") -B $buildDir -G "MinGW Makefiles" `
				"-DCMAKE_PREFIX_PATH=$($QtDir -replace '\\', '/')" `
				"-DCMAKE_C_COMPILER=$($MinGwBin -replace '\\', '/')/gcc.exe" `
				"-DCMAKE_CXX_COMPILER=$($MinGwBin -replace '\\', '/')/g++.exe" `
				"-DCMAKE_MAKE_PROGRAM=$($MinGwBin -replace '\\', '/')/mingw32-make.exe" `
				"-DCMAKE_BUILD_TYPE=Debug" 2>&1 | Out-String

			if($LASTEXITCODE -ne 0)
			{
				$pcOk = $false
				Report "FAIL" "pc build" "cmake configure failed"
				Indent $configOut
			}
		}

		if($pcOk)
		{
			$buildOut = & $CMake --build $buildDir -j 8 2>&1 | Out-String
			$seconds = [math]::Round($sw.Elapsed.TotalSeconds, 1)

			if($LASTEXITCODE -ne 0 -or -not (Test-Path $pcExe))
			{
				$pcOk = $false
				Report "FAIL" "pc build" "cmake --build failed, ${seconds}s"
				$lines = @($buildOut -split "\r?\n" | Where-Object { $_ -match 'error|Error|undefined|failed' })

				if($lines.Count -eq 0)
				{
					$lines = @($buildOut -split "\r?\n" | Select-Object -Last 30)
				}

				Indent ($lines -join "`n")
			}
			else
			{
				$pcWarnings = @($buildOut -split "\r?\n" | Where-Object { $_ -match 'warning:' })
				$note = "${seconds}s"

				if($pcWarnings.Count -gt 0)
				{
					$note += ", $($pcWarnings.Count) warning(s)"
				}

				Report "OK" "pc build" $note

				if($pcWarnings.Count -gt 0)
				{
					Indent ($pcWarnings -join "`n")
				}
			}
		}
	}

	# --- 5. golden frames ------------------------------------------------------

	$goldenDir = Join-Path $root "golden"
	$viewsPath = Join-Path $goldenDir "views.txt"
	$framesDir = Join-Path $work "frames"

	if(-not $pcOk)
	{
		Report "SKIP" "frames" "no PC build"
	}
	elseif(-not (Test-Path $viewsPath))
	{
		Report "SKIP" "frames" "no golden\views.txt"
	}
	else
	{
		Add-Type -AssemblyName System.Drawing

		if(-not (Test-Path $framesDir))
		{
			New-Item -ItemType Directory -Path $framesDir | Out-Null
		}

		$env:PATH = "$QtDir\bin;$env:PATH"
		$views = @()

		foreach($line in [IO.File]::ReadAllLines($viewsPath))
		{
			$trimmed = $line.Trim()

			if($trimmed -eq "" -or $trimmed.StartsWith("#"))
			{
				continue
			}

			$parts = $trimmed -split '\s+', 2

			if($parts.Count -lt 2)
			{
				Report "FAIL" "frames" "bad views.txt line: $trimmed"
				continue
			}

			$views += @{ Name = $parts[0]; Args = ($parts[1] -split '\s+') }
		}

		foreach($view in $views)
		{
			$rendered = Join-Path $framesDir "$($view.Name).png"
			$golden = Join-Path $goldenDir "$($view.Name).png"

			if(Test-Path $rendered)
			{
				Remove-Item $rendered -Force
			}

			# The host warns on stderr and carries on (a --pos on a non-walkable
			# cell, say). Under "Stop", 2>&1 turns that line into a terminating
			# error and takes the whole run down, so it is relaxed for this call.
			$ErrorActionPreference = "Continue"
			$renderOut = & $pcExe @($view.Args) --screenshot $rendered 2>&1 | Out-String
			$ErrorActionPreference = "Stop"

			if($LASTEXITCODE -ne 0 -or -not (Test-Path $rendered))
			{
				Report "FAIL" "frames" "$($view.Name): render failed ($($view.Args -join ' '))"
				Indent $renderOut
				continue
			}

			if($UpdateGolden)
			{
				if(-not (Test-Path $goldenDir))
				{
					New-Item -ItemType Directory -Path $goldenDir | Out-Null
				}

				Copy-Item $rendered $golden -Force
				Report "OK" "frames" "$($view.Name): golden updated"
				continue
			}

			if(-not (Test-Path $golden))
			{
				Report "WARN" "frames" "$($view.Name): no golden yet - run with -UpdateGolden to accept .verify\frames\$($view.Name).png"
				continue
			}

			$a = New-Object Drawing.Bitmap $rendered
			$b = New-Object Drawing.Bitmap $golden

			try
			{
				if($a.Width -ne $b.Width -or $a.Height -ne $b.Height)
				{
					Report "FAIL" "frames" "$($view.Name): size $($a.Width)x$($a.Height) vs golden $($b.Width)x$($b.Height)"
					continue
				}

				$differ = 0
				$minX = $a.Width; $minY = $a.Height; $maxX = -1; $maxY = -1

				for($y = 0; $y -lt $a.Height; $y++)
				{
					for($x = 0; $x -lt $a.Width; $x++)
					{
						if($a.GetPixel($x, $y).ToArgb() -ne $b.GetPixel($x, $y).ToArgb())
						{
							$differ++

							if($x -lt $minX) { $minX = $x }
							if($y -lt $minY) { $minY = $y }
							if($x -gt $maxX) { $maxX = $x }
							if($y -gt $maxY) { $maxY = $y }
						}
					}
				}

				if($differ -eq 0)
				{
					Report "OK" "frames" "$($view.Name): matches golden"
				}
				else
				{
					Report "FAIL" "frames" "$($view.Name): $differ pixel(s) differ in x $minX-$maxX, y $minY-$maxY (see .verify\frames\$($view.Name).png)"
				}
			}
			finally
			{
				$a.Dispose()
				$b.Dispose()
			}
		}
	}

	# --- summary ---------------------------------------------------------------

	$failed = @($results | Where-Object { $_.Status -eq "FAIL" })
	$warned = @($results | Where-Object { $_.Status -eq "WARN" })

	Write-Output ""

	if($failed.Count -gt 0)
	{
		Write-Output "verify: FAIL - $($failed.Count) step(s) failed"
		exit 1
	}

	if($warned.Count -gt 0)
	{
		Write-Output "verify: OK with $($warned.Count) warning(s)"
		exit 0
	}

	Write-Output "verify: OK"
	exit 0
}
finally
{
	if($lock -ne $null)
	{
		$lock.Close()
	}
}
