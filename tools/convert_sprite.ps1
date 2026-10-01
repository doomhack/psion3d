Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

Add-Type -AssemblyName System.Drawing

# Sprite format v2 (SPRITES.md): one file per sprite, its frames back to back,
# each frame exactly as loadSprite() keeps it in the sprite's segment and the
# frame cache - SPRITE_FRAME_BYTES in sprite.c:
#
#      0  1,024  pixels, row-major 2bpp, four a byte, low bits first
#   1024     64  per row, (first << 4) | last opaque 4-pixel group, 0xF0 if empty
#   1088      4  box: left, top, right, bottom (left/right multiples of 4)
#   1092      4  'S' 'P' 'R' 2
#   1096      1  frame count
#   1097      1  this frame's index
#   1098      6  zero
$FrameBytes = 1104
$MaxFrames = 8

$InputPath = ""
$Name = "spriteData"
$OutputPath = ""
$Raw = $false
$WhiteTransparent = $false

for($argIndex = 0; $argIndex -lt $args.Count; $argIndex++)
{
	$arg = $args[$argIndex]

	if($arg -eq "/f" -or $arg -eq "-Raw")
	{
		$Raw = $true
		continue
	}

	if($arg -eq "-OutputPath")
	{
		$argIndex++

		if($argIndex -ge $args.Count)
		{
			throw "-OutputPath requires a path."
		}

		$OutputPath = $args[$argIndex]
		continue
	}

	if($arg -eq "-WhiteTransparent")
	{
		$argIndex++

		if($argIndex -ge $args.Count)
		{
			throw "-WhiteTransparent requires true or false."
		}

		$WhiteTransparent = [System.Convert]::ToBoolean($args[$argIndex])
		continue
	}

	if($InputPath -eq "")
	{
		$InputPath = $arg
	}
	elseif($Raw -and $OutputPath -eq "")
	{
		$OutputPath = $arg
	}
	elseif($Name -eq "spriteData")
	{
		$Name = $arg
	}
	else
	{
		throw "Unexpected argument: $arg"
	}
}

if($InputPath -eq "")
{
	throw "Usage: convert_sprite.bat [/f] input.png|base [output.spr|arrayName] [-OutputPath path] [-WhiteTransparent true|false]`n" +
		"  A base path such as sprites\sci takes sci0.png, sci1.png ... until one is missing."
}

function Get-Sprite-PixelValue($color)
{
	if($color.A -eq 0)
	{
		return 0
	}

	if($color.R -eq 255 -and $color.G -eq 0 -and $color.B -eq 0)
	{
		return 0
	}

	if($color.R -eq 0 -and $color.G -eq 255 -and $color.B -eq 0)
	{
		return 3
	}

	if($color.R -ge 240 -and $color.G -ge 240 -and $color.B -ge 240)
	{
		if($WhiteTransparent)
		{
			return 0
		}

		return 3
	}

	if($color.R -le 32 -and $color.G -le 32 -and $color.B -le 32)
	{
		return 2
	}

	return 1
}

function Format-HexByte($value)
{
	return "0x{0:x2}" -f ($value -band 0xff)
}

function Get-FullPath($path)
{
	if([System.IO.Path]::IsPathRooted($path))
	{
		return [System.IO.Path]::GetFullPath($path)
	}

	return [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $path))
}

# One frame from one PNG of at most 64x64, centred, into $fileBytes.
function Add-SpriteFrame($pngPath, $frameIndex, $frameCount, $fileBytes)
{
	$bitmap = [System.Drawing.Bitmap]::new($pngPath)

	try
	{
		if($bitmap.Width -gt 64 -or $bitmap.Height -gt 64)
		{
			throw "$pngPath cannot exceed 64x64 pixels. Got $($bitmap.Width)x$($bitmap.Height)."
		}

		$xOffset = [int][Math]::Floor((64 - $bitmap.Width) / 2)
		$yOffset = [int][Math]::Floor((64 - $bitmap.Height) / 2)

		$minX = 64
		$minY = 64
		$maxX = 0
		$maxY = 0
		$rowFirst = New-Object int[] 64
		$rowLast = New-Object int[] 64

		for($y = 0; $y -lt 64; $y++)
		{
			$rowFirst[$y] = 16
			$rowLast[$y] = -1
		}

		# Render-ready row-major data: four horizontal 2bpp pixels per byte.
		for($y = 0; $y -lt 64; $y++)
		{
			for($x = 0; $x -lt 64; $x += 4)
			{
				$packed = 0

				for($bit = 0; $bit -lt 4; $bit++)
				{
					$sourceX = $x + $bit - $xOffset
					$sourceY = $y - $yOffset
					$pixel = 0

					if($sourceX -ge 0 -and $sourceX -lt $bitmap.Width -and
						$sourceY -ge 0 -and $sourceY -lt $bitmap.Height)
					{
						$pixel = Get-Sprite-PixelValue $bitmap.GetPixel($sourceX, $sourceY)
					}

					$packed = $packed -bor ($pixel -shl ($bit * 2))
				}

				$fileBytes.Add([byte]$packed)

				if($packed -ne 0)
				{
					$group = $x -shr 2

					if($x -lt $minX) { $minX = $x }
					if($x + 4 -gt $maxX) { $maxX = $x + 4 }
					if($y -lt $minY) { $minY = $y }
					if($y + 1 -gt $maxY) { $maxY = $y + 1 }
					if($group -lt $rowFirst[$y]) { $rowFirst[$y] = $group }
					if($group -gt $rowLast[$y]) { $rowLast[$y] = $group }
				}
			}
		}

		if($minX -eq 64)
		{
			$minX = 0
			$minY = 0
			$maxX = 0
			$maxY = 0
		}

		# The row spans the draw uses to skip empty rows and transparent ends.
		for($y = 0; $y -lt 64; $y++)
		{
			if($rowLast[$y] -lt 0)
			{
				$fileBytes.Add(0xf0)
			}
			else
			{
				$fileBytes.Add([byte](($rowFirst[$y] -shl 4) -bor $rowLast[$y]))
			}
		}

		$fileBytes.Add([byte]$minX)
		$fileBytes.Add([byte]$minY)
		$fileBytes.Add([byte]$maxX)
		$fileBytes.Add([byte]$maxY)

		$fileBytes.Add(0x53) # S
		$fileBytes.Add(0x50) # P
		$fileBytes.Add(0x52) # R
		$fileBytes.Add(0x02) # format version
		$fileBytes.Add([byte]$frameCount)
		$fileBytes.Add([byte]$frameIndex)

		for($pad = 0; $pad -lt 6; $pad++)
		{
			$fileBytes.Add(0)
		}
	}
	finally
	{
		$bitmap.Dispose()
	}
}

# A .png is a one-frame sprite; anything else is a base name whose frames are
# base0.png, base1.png ... until one is missing - the frame count goes in the file.
$frames = New-Object System.Collections.Generic.List[string]

if($InputPath -match '\.png$')
{
	$frames.Add((Resolve-Path -LiteralPath $InputPath).ProviderPath)
}
else
{
	for($frame = 0; $frame -le $MaxFrames; $frame++)
	{
		$candidate = "$InputPath$frame.png"

		if(-not (Test-Path -LiteralPath $candidate))
		{
			break
		}

		$frames.Add((Resolve-Path -LiteralPath $candidate).ProviderPath)
	}

	if($frames.Count -eq 0)
	{
		throw "No frames found: expected ${InputPath}0.png."
	}
}

if($frames.Count -gt $MaxFrames)
{
	throw "A sprite has at most $MaxFrames frames; ${InputPath}$MaxFrames.png exists."
}

$fileBytes = New-Object System.Collections.Generic.List[byte]

for($frame = 0; $frame -lt $frames.Count; $frame++)
{
	Add-SpriteFrame $frames[$frame] $frame $frames.Count $fileBytes
}

if($fileBytes.Count -ne $frames.Count * $FrameBytes)
{
	throw "Internal error: $($fileBytes.Count) bytes for $($frames.Count) frames."
}

$rawBytes = $fileBytes.ToArray()

if($Raw)
{
	if($OutputPath -ne "")
	{
		$fullOutputPath = Get-FullPath $OutputPath
		$outputDir = Split-Path -Parent $fullOutputPath

		if($outputDir -ne "" -and !(Test-Path -LiteralPath $outputDir))
		{
			New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
		}

		[System.IO.File]::WriteAllBytes($fullOutputPath, $rawBytes)
		return
	}

	$stdout = [Console]::OpenStandardOutput()
	$stdout.Write($rawBytes, 0, $rawBytes.Length)
	$stdout.Flush()
	return
}

$lines = New-Object System.Collections.Generic.List[string]
$lines.Add("static const u8 $Name[] =")
$lines.Add("{")

for($i = 0; $i -lt $fileBytes.Count; $i += 16)
{
	$slice = $fileBytes.GetRange($i, 16) | ForEach-Object { Format-HexByte $_ }
	$suffix = if($i + 16 -lt $fileBytes.Count) { "," } else { "" }
	$lines.Add("`t" + ($slice -join ", ") + $suffix)
}

$lines.Add("};")

$text = $lines -join "`r`n"

if($OutputPath -ne "")
{
	$fullOutputPath = Get-FullPath $OutputPath
	$outputDir = Split-Path -Parent $fullOutputPath

	if($outputDir -ne "" -and !(Test-Path -LiteralPath $outputDir))
	{
		New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
	}

	[System.IO.File]::WriteAllText($fullOutputPath, $text + "`r`n", [System.Text.Encoding]::ASCII)
	return
}

Write-Output $text
