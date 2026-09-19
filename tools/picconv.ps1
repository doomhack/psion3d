Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

# Psion .PIC <-> PNG converter.
#
#   picconv.ps1 Screen.pic [out.png] [-Split]
#   picconv.ps1 image.png  [out.pic] [-Mono]
#
# The direction is chosen by the input extension. A .PIC holding two bitmaps
# of the same size is treated as a screen grab: bitmap 0 is the black plane,
# bitmap 1 the grey plane, and they composite to a white/grey/black PNG the
# way the LCD shows them (black wins where both are set). -Split writes every
# bitmap to its own PNG instead (out_0.png, out_1.png, ...). Any other bitmap
# count is always split.
#
# PNG -> PIC quantises luminance into three levels and writes a two-plane
# file; -Mono writes a single bitmap with every dark pixel set.
#
# Format (PSIONICS BITMAP.FMT): 8-byte header "PIC" $DC $30 $30 <count:word>,
# then <count> 12-byte descriptors (crc, width, height, size, offset from the
# end of this descriptor to the data), then the row-major data. Rows are
# padded to an even byte count and bit 0 is the leftmost pixel. The CRC is
# CRC-16/XMODEM (poly $1021, init 0, MSB-first) over the bitmap data only.

Add-Type -AssemblyName System.Drawing

$InputPath = ""
$OutputPath = ""
$Split = $false
$Mono = $false

for($argIndex = 0; $argIndex -lt $args.Count; $argIndex++)
{
	$arg = $args[$argIndex]

	if($arg -eq "-Split")
	{
		$Split = $true
		continue
	}

	if($arg -eq "-Mono")
	{
		$Mono = $true
		continue
	}

	if($InputPath -eq "")
	{
		$InputPath = $arg
		continue
	}

	if($OutputPath -eq "")
	{
		$OutputPath = $arg
		continue
	}

	throw "Unexpected argument: $arg"
}

if($InputPath -eq "")
{
	throw "Usage: picconv.ps1 <in.pic|in.png> [out] [-Split] [-Mono]"
}

$InputPath = (Resolve-Path $InputPath).Path

function Get-Crc16Xmodem([byte[]] $data)
{
	$crc = 0

	foreach($b in $data)
	{
		$crc = $crc -bxor ([int]$b -shl 8)

		for($i = 0; $i -lt 8; $i++)
		{
			if($crc -band 0x8000)
			{
				$crc = (($crc -shl 1) -bxor 0x1021) -band 0xFFFF
			}
			else
			{
				$crc = ($crc -shl 1) -band 0xFFFF
			}
		}
	}

	return $crc
}

function Get-RowBytes([int] $width)
{
	return ((($width + 7) -shr 3) + 1) -band -bnot 1
}

function Read-Pic([string] $path)
{
	$bytes = [IO.File]::ReadAllBytes($path)

	if($bytes.Length -lt 8 -or $bytes[0] -ne 0x50 -or $bytes[1] -ne 0x49 -or $bytes[2] -ne 0x43 -or $bytes[3] -ne 0xDC)
	{
		throw "$path is not a PIC file (bad signature)."
	}

	$count = [BitConverter]::ToUInt16($bytes, 6)
	$bitmaps = @()

	for($index = 0; $index -lt $count; $index++)
	{
		$rec = 8 + $index * 12
		$crc = [BitConverter]::ToUInt16($bytes, $rec)
		$width = [BitConverter]::ToUInt16($bytes, $rec + 2)
		$height = [BitConverter]::ToUInt16($bytes, $rec + 4)
		$size = [BitConverter]::ToUInt16($bytes, $rec + 6)
		$offset = [BitConverter]::ToInt32($bytes, $rec + 8)
		$start = $rec + 12 + $offset

		if($start + $size -gt $bytes.Length)
		{
			throw "Bitmap $index data runs past the end of the file."
		}

		$data = New-Object byte[] $size
		[Array]::Copy($bytes, $start, $data, 0, $size)

		$actual = Get-Crc16Xmodem $data

		if($actual -ne $crc)
		{
			Write-Warning ("Bitmap {0}: stored CRC {1:X4} != computed {2:X4}" -f $index, $crc, $actual)
		}

		$bitmaps += ,(New-Object PSObject -Property @{ Width = $width; Height = $height; Data = $data })
	}

	return ,$bitmaps
}

# Builds a 24bpp bitmap from a per-pixel shade callback: 0 white, 1 grey, 2 black.
function New-ShadeBitmap([int] $width, [int] $height, [byte[]] $shades)
{
	$bmp = New-Object System.Drawing.Bitmap $width, $height, ([System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
	$rect = New-Object System.Drawing.Rectangle 0, 0, $width, $height
	$locked = $bmp.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::WriteOnly, $bmp.PixelFormat)
	$stride = $locked.Stride
	$buffer = New-Object byte[] ($stride * $height)
	$levels = [byte[]] @(255, 128, 0)

	for($y = 0; $y -lt $height; $y++)
	{
		$row = $y * $stride
		$src = $y * $width

		for($x = 0; $x -lt $width; $x++)
		{
			$v = $levels[$shades[$src + $x]]
			$p = $row + $x * 3
			$buffer[$p] = $v
			$buffer[$p + 1] = $v
			$buffer[$p + 2] = $v
		}
	}

	[System.Runtime.InteropServices.Marshal]::Copy($buffer, 0, $locked.Scan0, $buffer.Length)
	$bmp.UnlockBits($locked)

	return $bmp
}

# Returns one shade byte per pixel from one (mono) or two (black, grey) planes.
function Get-Shades($black, $grey)
{
	$width = $black.Width
	$height = $black.Height
	$rowBytes = Get-RowBytes $width
	$shades = New-Object byte[] ($width * $height)

	for($y = 0; $y -lt $height; $y++)
	{
		$row = $y * $rowBytes
		$dst = $y * $width

		for($x = 0; $x -lt $width; $x++)
		{
			$bit = 1 -shl ($x -band 7)
			$i = $row + ($x -shr 3)

			if($black.Data[$i] -band $bit)
			{
				$shades[$dst + $x] = 2
			}
			elseif($grey -ne $null -and ($grey.Data[$i] -band $bit))
			{
				$shades[$dst + $x] = 1
			}
		}
	}

	return ,$shades
}

function Convert-PicToPng([string] $inPath, [string] $outPath)
{
	if($outPath -eq "")
	{
		$outPath = [IO.Path]::ChangeExtension($inPath, ".png")
	}

	$bitmaps = Read-Pic $inPath
	$isScreen = ($bitmaps.Count -eq 2) -and ($bitmaps[0].Width -eq $bitmaps[1].Width) -and ($bitmaps[0].Height -eq $bitmaps[1].Height)

	if($isScreen -and -not $Split)
	{
		$shades = Get-Shades $bitmaps[0] $bitmaps[1]
		$bmp = New-ShadeBitmap $bitmaps[0].Width $bitmaps[0].Height $shades
		$bmp.Save($outPath, [System.Drawing.Imaging.ImageFormat]::Png)
		$bmp.Dispose()
		Write-Host ("{0}: {1}x{2}, black + grey planes -> {3}" -f (Split-Path $inPath -Leaf), $bitmaps[0].Width, $bitmaps[0].Height, $outPath)
		return
	}

	$stem = [IO.Path]::Combine([IO.Path]::GetDirectoryName($outPath), [IO.Path]::GetFileNameWithoutExtension($outPath))

	for($index = 0; $index -lt $bitmaps.Count; $index++)
	{
		$bm = $bitmaps[$index]
		$path = if($bitmaps.Count -eq 1) { $outPath } else { "{0}_{1}.png" -f $stem, $index }
		$shades = Get-Shades $bm $null
		$bmp = New-ShadeBitmap $bm.Width $bm.Height $shades
		$bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
		$bmp.Dispose()
		Write-Host ("{0}: bitmap {1} {2}x{3} -> {4}" -f (Split-Path $inPath -Leaf), $index, $bm.Width, $bm.Height, $path)
	}
}

function Convert-PngToPic([string] $inPath, [string] $outPath)
{
	if($outPath -eq "")
	{
		$outPath = [IO.Path]::ChangeExtension($inPath, ".pic")
	}

	$bmp = New-Object System.Drawing.Bitmap $inPath
	$width = $bmp.Width
	$height = $bmp.Height
	$rowBytes = Get-RowBytes $width
	$size = $rowBytes * $height
	$black = New-Object byte[] $size
	$grey = New-Object byte[] $size

	for($y = 0; $y -lt $height; $y++)
	{
		$row = $y * $rowBytes

		for($x = 0; $x -lt $width; $x++)
		{
			$c = $bmp.GetPixel($x, $y)

			# Transparent counts as background (white).
			if($c.A -lt 128)
			{
				continue
			}

			$lum = (($c.R * 299) + ($c.G * 587) + ($c.B * 114)) / 1000
			$i = $row + ($x -shr 3)
			$bit = 1 -shl ($x -band 7)

			if($Mono)
			{
				if($lum -lt 128)
				{
					$black[$i] = $black[$i] -bor $bit
				}
			}
			elseif($lum -lt 85)
			{
				$black[$i] = $black[$i] -bor $bit
			}
			elseif($lum -lt 170)
			{
				$grey[$i] = $grey[$i] -bor $bit
			}
		}
	}

	$bmp.Dispose()

	# Built element by element: @(,$black) unrolls the byte array in PS 5.1.
	$count = if($Mono) { 1 } else { 2 }
	$planes = New-Object object[] $count
	$planes[0] = $black

	if(-not $Mono)
	{
		$planes[1] = $grey
	}
	$out = New-Object IO.MemoryStream
	$w = New-Object IO.BinaryWriter $out

	$w.Write([byte[]] @(0x50, 0x49, 0x43, 0xDC, 0x30, 0x30))
	$w.Write([uint16] $count)

	for($index = 0; $index -lt $count; $index++)
	{
		# Data follows all descriptors, so bitmap N sits (count - 1 - N)
		# descriptors plus the earlier bitmaps' data past the end of its record.
		$offset = ($count - 1 - $index) * 12 + $index * $size

		$w.Write([uint16] (Get-Crc16Xmodem $planes[$index]))
		$w.Write([uint16] $width)
		$w.Write([uint16] $height)
		$w.Write([uint16] $size)
		$w.Write([int32] $offset)
	}

	foreach($plane in $planes)
	{
		$w.Write($plane)
	}

	$w.Flush()
	[IO.File]::WriteAllBytes($outPath, $out.ToArray())
	$w.Dispose()

	Write-Host ("{0}: {1}x{2}, {3} plane(s) -> {4}" -f (Split-Path $inPath -Leaf), $width, $height, $count, $outPath)
}

$ext = [IO.Path]::GetExtension($InputPath).ToLowerInvariant()

if($ext -eq ".pic")
{
	Convert-PicToPng $InputPath $OutputPath
}
else
{
	Convert-PngToPic $InputPath $OutputPath
}
