# profile.ps1 - build the profiling image, PROF3D.IMG.
#
# The profiling build is the normal game with BENCH_PROFILE defined (see
# bench.h): Options > Benchmark then measures every station in eight passes,
# each switching part of the frame off or doing it twice, and ends on a
# table of what each part costs in milliseconds.
#
# It is built from a copy of the sources in profbld\ (gitignored), never
# beside them. tsc /m decides what to rebuild by timestamp, not by defines,
# so building both flavours in one directory would leave OBJs of one in the
# other. PSION3D.IMG and its OBJs are not touched.
#
#   .\tools\profile.bat          build, copy to PROF3D.IMG, report DGROUP
#
# Exit codes: 0 built, 1 the build failed.

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$tools = Join-Path $root "tools"
$work = Join-Path $root "profbld"
$out = Join-Path $root "PROF3D.IMG"

$DosBox = "C:\Program Files (x86)\DOSBox-0.74-3\DOSBox.exe"

if(-not (Test-Path $DosBox))
{
	Write-Output "profile: DOSBox not found at $DosBox"
	exit 1
}

if(-not (Test-Path $work))
{
	New-Item -ItemType Directory -Path $work | Out-Null
}

# The sources, fresh each time. Copy-Item keeps the timestamps, so tsc /m
# still rebuilds only what changed since the last profiling build.
Get-ChildItem -Path $root -File | Where-Object { $_.Extension -in ".c", ".h", ".a" } |
	ForEach-Object { Copy-Item $_.FullName (Join-Path $work $_.Name) -Force }

# unnamed.pr with the define added ahead of the first #compile, so it
# applies to every module. Rewritten only when it differs: a new timestamp
# on the project file would not rebuild anything, but there is no reason to
# look as if it might.
$prRegex = [regex]"(?m)^#compile"
$pr = $prRegex.Replace([IO.File]::ReadAllText((Join-Path $root "unnamed.pr")), "#pragma define(BENCH_PROFILE=>1)`n#compile", 1)

$prPath = Join-Path $work "unnamed.pr"

if(-not (Test-Path $prPath) -or [IO.File]::ReadAllText($prPath) -ne $pr)
{
	[IO.File]::WriteAllText($prPath, $pr)
}

$imgPath = Join-Path $work "PSION3D.IMG"
$exePath = Join-Path $work "PSION3D.EXE"
$logPath = Join-Path $work "build.log"

if(Test-Path $exePath)
{
	Remove-Item $exePath
}

# Through the conf's D: mount of the repo. A mount of our own from -c
# writes nothing, and DOSBox does not list .verify at all.
$sw = [Diagnostics.Stopwatch]::StartNew()
& $DosBox -c "D:" -c "cd \profbld" -c "tsc /m unnamed.pr /smain=psion3d /v0 /zq > D:\profbld\build.log" -c "exit" | Out-Null
$seconds = [math]::Round($sw.Elapsed.TotalSeconds, 1)

$log = ""

if(Test-Path $logPath)
{
	$log = [IO.File]::ReadAllText($logPath)
}

$errors = @($log -split "\r?\n" | Where-Object { $_ -match 'Error' })
$warnings = @($log -split "\r?\n" | Where-Object { $_ -match 'Warning' })

foreach($line in ($errors + $warnings))
{
	Write-Output "        $line"
}

if($errors.Count -gt 0 -or -not (Test-Path $exePath))
{
	Write-Output "profile: build failed, ${seconds}s - see $logPath"
	exit 1
}

Copy-Item $imgPath $out -Force

$hash = (Get-FileHash $out).Hash.Substring(0, 12).ToLower()
Write-Output "profile: PROF3D.IMG $hash built in ${seconds}s, $($warnings.Count) warning(s)"

# DGROUP of the profiling build: it carries a few hundred bytes the normal
# build does not, and it has to fit as well.
& (Join-Path $tools "memcheck.ps1") -Map (Join-Path $work "PSION3D.MAP") |
	Where-Object { $_ -match "^Near data" } | ForEach-Object { "profile: $($_ -replace '\s+', ' ')" }

exit 0
