Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

Add-Type -AssemblyName System.Drawing

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
	throw "Usage: convert_sprite.bat [/f] input.png [output.spr|arrayName] [-OutputPath path] [-WhiteTransparent true|false]"
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

$resolvedInput = Resolve-Path -LiteralPath $InputPath
$bitmap = [System.Drawing.Bitmap]::new($resolvedInput.ProviderPath)

try
{
	if($bitmap.Width -gt 64 -or $bitmap.Height -gt 64)
	{
		throw "Input image cannot exceed 64x64 pixels. Got $($bitmap.Width)x$($bitmap.Height)."
	}

	$xOffset = [int][Math]::Floor((64 - $bitmap.Width) / 2)
	$yOffset = [int][Math]::Floor((64 - $bitmap.Height) / 2)

	$bytes = New-Object System.Collections.Generic.List[byte]
	$minX = 64
	$minY = 64
	$maxX = 0
	$maxY = 0
	$bandLeft = @(16, 16, 16, 16, 16, 16, 16, 16)
	$bandRight = @(-1, -1, -1, -1, -1, -1, -1, -1)

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

			$bytes.Add([byte]$packed)

			if($packed -ne 0)
			{
				$band = $y -shr 3
				$group = $x -shr 2

				if($x -lt $minX) { $minX = $x }
				if($x + 4 -gt $maxX) { $maxX = $x + 4 }
				if($y -lt $minY) { $minY = $y }
				if($y + 1 -gt $maxY) { $maxY = $y + 1 }
				if($group -lt $bandLeft[$band]) { $bandLeft[$band] = $group }
				if($group -gt $bandRight[$band]) { $bandRight[$band] = $group }
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

	$fileBytes = New-Object System.Collections.Generic.List[byte]
	$fileBytes.Add(0x53) # S
	$fileBytes.Add(0x50) # P
	$fileBytes.Add(0x52) # R
	$fileBytes.Add(0x01) # format version
	$fileBytes.Add([byte]$minX)
	$fileBytes.Add([byte]$minY)
	$fileBytes.Add([byte]$maxX)
	$fileBytes.Add([byte]$maxY)

	for($band = 0; $band -lt 8; $band++)
	{
		if($bandRight[$band] -lt 0)
		{
			$fileBytes.Add(0xf0)
		}
		else
		{
			$fileBytes.Add([byte](($bandLeft[$band] -shl 4) -bor $bandRight[$band]))
		}
	}

	foreach($value in $bytes)
	{
		$fileBytes.Add($value)
	}

	$rawBytes = $fileBytes.ToArray()

	if($Raw)
	{
		if($OutputPath -ne "")
		{
			if([System.IO.Path]::IsPathRooted($OutputPath))
			{
				$fullOutputPath = [System.IO.Path]::GetFullPath($OutputPath)
			}
			else
			{
				$fullOutputPath = [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $OutputPath))
			}

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
		if([System.IO.Path]::IsPathRooted($OutputPath))
		{
			$fullOutputPath = [System.IO.Path]::GetFullPath($OutputPath)
		}
		else
		{
			$fullOutputPath = [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $OutputPath))
		}

		$outputDir = Split-Path -Parent $fullOutputPath

		if($outputDir -ne "" -and !(Test-Path -LiteralPath $outputDir))
		{
			New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
		}

		[System.IO.File]::WriteAllText($fullOutputPath, $text + "`r`n", [System.Text.Encoding]::ASCII)
		return
	}

	Write-Output $text
}
finally
{
	$bitmap.Dispose()
}
