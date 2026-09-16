# memcheck.ps1 - DGROUP budget check for the Psion build.
#
# Reads PSION3D.MAP (the TopSpeed linker map) and prints near code, near data
# (DGROUP) and far sprite usage, with the delta against a baseline: either the
# last row of the history table in MEMORY_BUDGET.md, or a saved copy of an
# earlier PSION3D.MAP passed with -Baseline. Exits non-zero when DGROUP is over
# the limit, so it can gate a build.
#
#   .\tools\memcheck.bat                          report, compare to last history row
#   .\tools\memcheck.bat -Baseline old.MAP        per-segment and per-symbol deltas
#   .\tools\memcheck.bat -Record "what changed"   append a row to MEMORY_BUDGET.md
#   .\tools\memcheck.bat -Limit 50000             DGROUP limit in bytes (default 48 KB)
#
# Exit codes: 0 within limit, 1 over limit, 2 map missing or unreadable.

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)

$MapPath = Join-Path $root "PSION3D.MAP"
$BudgetPath = Join-Path $root "MEMORY_BUDGET.md"
$BaselinePath = ""
$RecordNote = $null
$Limit = 48 * 1024
$SegmentSize = 64 * 1024

for($argIndex = 0; $argIndex -lt $args.Count; $argIndex++)
{
	$arg = $args[$argIndex]

	if($arg -eq "-Map" -or $arg -eq "-Baseline" -or $arg -eq "-Record" -or $arg -eq "-Limit" -or $arg -eq "-Budget")
	{
		$argIndex++

		if($argIndex -ge $args.Count)
		{
			throw "$arg requires a value."
		}

		$value = $args[$argIndex]

		switch($arg)
		{
			"-Map" { $MapPath = $value }
			"-Baseline" { $BaselinePath = $value }
			"-Record" { $RecordNote = $value }
			"-Limit" { $Limit = [int]$value }
			"-Budget" { $BudgetPath = $value }
		}

		continue
	}

	throw "Unknown argument: $arg"
}

# --- map parsing -------------------------------------------------------------

# Returns @{ Segments = ordered list of @{Name,Class,Group,Start,Stop,Length};
#            Publics = ordered list of @{Seg,Off,Name} }
function Read-LinkerMap($path)
{
	if(-not (Test-Path $path))
	{
		throw "Map file not found: $path (build first)"
	}

	$segments = @()
	$publics = @()

	foreach($line in [IO.File]::ReadAllLines($path))
	{
		if($line -match '^\s*([0-9A-F]+)H\s+([0-9A-F]+)H\s+([0-9A-F]+)H\s+(\S+)\s+(\S+)\s+(\S+)')
		{
			$segments += @{
				Start = [Convert]::ToInt32($matches[1], 16)
				Stop = [Convert]::ToInt32($matches[2], 16)
				Length = [Convert]::ToInt32($matches[3], 16)
				Name = $matches[4]
				Class = $matches[5]
				Group = $matches[6]
			}
			continue
		}

		if($line -match '^\s*([0-9A-F]{4}):([0-9A-F]{4})\s+(\S+)\s*$')
		{
			$publics += @{
				Seg = [Convert]::ToInt32($matches[1], 16)
				Off = [Convert]::ToInt32($matches[2], 16)
				Name = $matches[3]
			}
		}
	}

	if($segments.Count -eq 0)
	{
		throw "No segment table found in $path"
	}

	return @{ Segments = $segments; Publics = $publics }
}

# The numbers the budget doc tracks, plus the per-segment breakdown.
function Measure-Map($map)
{
	$text = 0
	$dgroupSegments = @()

	foreach($seg in $map.Segments)
	{
		if($seg.Name -eq "_TEXT") { $text = $seg.Length }
		if($seg.Group -eq "DGROUP") { $dgroupSegments += $seg }
	}

	if($dgroupSegments.Count -eq 0)
	{
		throw "No DGROUP segments in map"
	}

	# DGROUP used is __bss_end relative to the group's segment paragraph, the
	# same reading as the recipe in MEMORY_BUDGET.md. Fall back to the segment
	# table if the symbol is missing.
	$bssEnd = $map.Publics | Where-Object { $_.Name -eq "__bss_end" } | Select-Object -First 1

	if($bssEnd -ne $null)
	{
		$dgroup = $bssEnd.Off
		$dgroupSeg = $bssEnd.Seg
	}
	else
	{
		$first = $dgroupSegments[0].Start
		$last = ($dgroupSegments | ForEach-Object { $_.Stop } | Measure-Object -Maximum).Maximum
		$dgroup = $last - $first
		$dgroupSeg = $first -shr 4
	}

	# DGROUP publics with the size of the region each one starts, so two maps
	# can be compared symbol by symbol. The linker only publishes externs;
	# statics fold into the gap after the preceding public.
	$dgroupPublics = @($map.Publics | Where-Object { $_.Seg -eq $dgroupSeg } | Sort-Object { $_.Off })
	$regions = @{}

	for($i = 0; $i -lt $dgroupPublics.Count; $i++)
	{
		$sym = $dgroupPublics[$i]
		$next = $dgroup

		if($i + 1 -lt $dgroupPublics.Count)
		{
			$next = $dgroupPublics[$i + 1].Off
		}

		$regions[$sym.Name] = @{ Off = $sym.Off; Size = ($next - $sym.Off) }
	}

	return @{
		Text = $text
		DGroup = $dgroup
		Segments = $dgroupSegments
		Regions = $regions
	}
}

# Far sprite bytes: every loadSprite() call in game_map.c, resolved against the
# frame files in spr/ the same way the loader does - frame 0 then consecutive
# frames until one is missing, 1,024 bytes each.
function Measure-FarSprites($root)
{
	$source = Join-Path $root "game_map.c"
	$sprDir = Join-Path $root "spr"

	if(-not (Test-Path $source) -or -not (Test-Path $sprDir))
	{
		return 0
	}

	$frames = 0

	foreach($m in [regex]::Matches([IO.File]::ReadAllText($source), 'loadSprite\s*\(\s*"([^"]+)"'))
	{
		$base = $m.Groups[1].Value

		for($frame = 0; $frame -lt 8; $frame++)
		{
			if(-not (Test-Path (Join-Path $sprDir "$base$frame.spr")))
			{
				break
			}

			$frames++
		}
	}

	return $frames * 1024
}

# --- baseline ----------------------------------------------------------------

# Last row of the History table: @{ Date, Text, DGroup, Far, Note } or $null.
function Read-LastHistoryRow($budgetPath)
{
	if(-not (Test-Path $budgetPath))
	{
		return $null
	}

	$row = $null

	foreach($line in [IO.File]::ReadAllLines($budgetPath))
	{
		if($line -match '^\|\s*(\d{4}-\d{2}-\d{2})\s*\|\s*([\d,]+)\s*\|\s*([\d,]+)\s*\|\s*([\d,]+)\s*\|\s*(.*?)\s*\|\s*$')
		{
			$row = @{
				Date = $matches[1]
				Text = [int]($matches[2] -replace ',', '')
				DGroup = [int]($matches[3] -replace ',', '')
				Far = [int]($matches[4] -replace ',', '')
				Note = $matches[5]
			}
		}
	}

	return $row
}

function Add-HistoryRow($budgetPath, $date, $text, $dgroup, $far, $note)
{
	$raw = [IO.File]::ReadAllText($budgetPath)
	$eol = "`n"

	if($raw.Contains("`r`n"))
	{
		$eol = "`r`n"
	}

	$lines = $raw -split "\r?\n"
	$lastRow = -1

	for($i = 0; $i -lt $lines.Count; $i++)
	{
		if($lines[$i] -match '^\|\s*\d{4}-\d{2}-\d{2}\s*\|')
		{
			$lastRow = $i
		}
	}

	if($lastRow -lt 0)
	{
		throw "No history table rows found in $budgetPath"
	}

	$newRow = "| $date | $('{0:N0}' -f $text) | $('{0:N0}' -f $dgroup) | $('{0:N0}' -f $far) | $note |"
	$out = @($lines[0..$lastRow]) + @($newRow) + @($lines[($lastRow + 1)..($lines.Count - 1)])

	# Keep the file's own line endings and write UTF-8 without a BOM.
	[IO.File]::WriteAllText($budgetPath, ($out -join $eol), (New-Object Text.UTF8Encoding($false)))
}

# --- report ------------------------------------------------------------------

function Format-Delta($delta)
{
	if($delta -eq 0) { return "      -" }
	if($delta -gt 0) { return ('{0,7}' -f ('+' + ('{0:N0}' -f $delta))) }
	return ('{0,7}' -f ('{0:N0}' -f $delta))
}

function Write-Row($label, $value, $delta, $note)
{
	$deltaText = ""

	if($delta -ne $null)
	{
		$deltaText = Format-Delta $delta
	}

	Write-Output ('{0,-22} {1,8}  {2,7}  {3}' -f $label, ('{0:N0}' -f $value), $deltaText, $note)
}

try
{
	$map = Read-LinkerMap $MapPath
}
catch
{
	Write-Output "memcheck: $($_.Exception.Message)"
	exit 2
}

$now = Measure-Map $map
$far = Measure-FarSprites $root

$base = $null
$baseLabel = ""
$baseRegions = $null

if($BaselinePath -ne "")
{
	try
	{
		$baseMeasured = Measure-Map (Read-LinkerMap $BaselinePath)
	}
	catch
	{
		Write-Output "memcheck: $($_.Exception.Message)"
		exit 2
	}

	$base = @{ Text = $baseMeasured.Text; DGroup = $baseMeasured.DGroup; Far = $far }
	$baseRegions = $baseMeasured
	$baseLabel = "vs " + (Split-Path -Leaf $BaselinePath)
}
else
{
	$row = Read-LastHistoryRow $BudgetPath

	if($row -ne $null)
	{
		$base = $row
		$baseLabel = "vs $($row.Date) `"$($row.Note)`""
	}
}

function Delta-Or-Null($key)
{
	if($base -eq $null) { return $null }
	return $now.$key - $base.$key
}

$textDelta = Delta-Or-Null "Text"
$dgroupDelta = Delta-Or-Null "DGroup"
$farDelta = $null

if($base -ne $null)
{
	$farDelta = $far - $base.Far
}

$free = $Limit - $now.DGroup
$percent = [math]::Round(100.0 * $now.DGroup / $SegmentSize, 1)

Write-Output ("memcheck: {0}  {1}" -f (Split-Path -Leaf $MapPath), $baseLabel)
Write-Output ""
Write-Row "Near code (_TEXT)" $now.Text $textDelta ("of {0:N0}" -f $SegmentSize)
Write-Row "Near data (DGROUP)" $now.DGroup $dgroupDelta ("of {0:N0} ({1}%), limit {2:N0}, {3:N0} to limit" -f $SegmentSize, $percent, $Limit, $free)
Write-Row "Far sprites" $far $farDelta "loadSprite() frames x 1,024, from spr/"
Write-Output ""

foreach($seg in $now.Segments)
{
	$segDelta = $null

	if($baseRegions -ne $null)
	{
		$baseSeg = $baseRegions.Segments | Where-Object { $_.Name -eq $seg.Name } | Select-Object -First 1
		$segDelta = 0

		if($baseSeg -ne $null)
		{
			$segDelta = $seg.Length - $baseSeg.Length
		}
	}

	Write-Row ("  " + $seg.Name) $seg.Length $segDelta $seg.Class
}

# With a baseline map, name the DGROUP regions that changed size. A region is
# a public symbol plus the statics that follow it up to the next public.
if($baseRegions -ne $null)
{
	$changed = @()

	foreach($name in $now.Regions.Keys)
	{
		$size = $now.Regions[$name].Size
		$oldSize = 0

		if($baseRegions.Regions.ContainsKey($name))
		{
			$oldSize = $baseRegions.Regions[$name].Size
		}

		if($size -ne $oldSize)
		{
			$changed += @{ Name = $name; Size = $size; Delta = ($size - $oldSize) }
		}
	}

	foreach($name in $baseRegions.Regions.Keys)
	{
		if(-not $now.Regions.ContainsKey($name))
		{
			$changed += @{ Name = $name; Size = 0; Delta = -$baseRegions.Regions[$name].Size }
		}
	}

	if($changed.Count -gt 0)
	{
		Write-Output ""
		Write-Output "DGROUP regions that changed (public symbol plus the statics after it):"

		foreach($c in ($changed | Sort-Object { -[math]::Abs($_.Delta) }))
		{
			Write-Row ("  " + $c.Name) $c.Size $c.Delta ""
		}
	}
}

if($RecordNote -ne $null)
{
	$date = Get-Date -Format "yyyy-MM-dd"
	Add-HistoryRow $BudgetPath $date $now.Text $now.DGroup $far $RecordNote
	Write-Output ""
	Write-Output "Recorded: | $date | $('{0:N0}' -f $now.Text) | $('{0:N0}' -f $now.DGroup) | $('{0:N0}' -f $far) | $RecordNote |"
}

if($now.DGroup -gt $Limit)
{
	Write-Output ""
	Write-Output ("memcheck: FAIL - DGROUP {0:N0} exceeds limit {1:N0} by {2:N0} bytes" -f $now.DGroup, $Limit, ($now.DGroup - $Limit))
	exit 1
}

exit 0
