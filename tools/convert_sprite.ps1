Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

Add-Type -AssemblyName System.Drawing

$InputPath = ""
$Name = "spriteData"
$OutputPath = ""
$Raw = $false
$WhiteTransparent = $true

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

	if($WhiteTransparent -and $color.R -ge 240 -and $color.G -ge 240 -and $color.B -ge 240)
	{
		return 0
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
	if($bitmap.Width -ne 64 -or $bitmap.Height -ne 64)
	{
		throw "Input image must be exactly 64x64 pixels. Got $($bitmap.Width)x$($bitmap.Height)."
	}

	$bytes = New-Object System.Collections.Generic.List[byte]

	for($x = 0; $x -lt 64; $x++)
	{
		for($y = 0; $y -lt 64; $y += 4)
		{
			$packed = 0

			for($bit = 0; $bit -lt 4; $bit++)
			{
				$pixel = Get-Sprite-PixelValue $bitmap.GetPixel($x, $y + $bit)
				$packed = $packed -bor ($pixel -shl ($bit * 2))
			}

			$bytes.Add([byte]$packed)
		}
	}

	$rawBytes = $bytes.ToArray()

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
	$lines.Add("static u8 $Name[SPRITE_BYTES] =")
	$lines.Add("{")

	for($i = 0; $i -lt $bytes.Count; $i += 16)
	{
		$slice = $bytes.GetRange($i, 16) | ForEach-Object { Format-HexByte $_ }
		$suffix = if($i + 16 -lt $bytes.Count) { "," } else { "" }
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
