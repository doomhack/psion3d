Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

# Series 3a sound file (.wve), PLIB Reference "Sound files" (SndFile in epoc.h):
#
#      0  16  "ALawSoundFile**" and its zero terminator
#     16   2  version, 0x100F as the SDK's wav2wve writes it
#     18   4  sample count, always the file size less this header
#     22   2  silence appended to each repeat, in 1/32s ticks
#     24   2  repeats (0 and 1 both play once)
#     26   6  spare, zero
#     32      samples, one A-law byte each, played at 8000 a second
#
# The pipeline is: read a PCM or float .wav, mix to mono, resample to 8kHz
# through a windowed-sinc low-pass, scale to 13 bits and A-law encode.
#
# Level: -Gain 100 matches wav2wve, which maps 16-bit full scale to +-2048,
# half the codec's 13-bit range; -Gain 200 uses all of it. -Normalize scales
# the loudest sample to 4095 instead. Samples past +-4095 are clipped and
# counted.

Add-Type -TypeDefinition @"
using System;
using System.IO;
using System.Text;

public static class WveConverter
{
	public const int OutRate = 8000;

	public static double[] ReadWav(string path, out int rate, out int channels, out int bits, out int format)
	{
		byte[] b = File.ReadAllBytes(path);
		rate = 0; channels = 0; bits = 0; format = 0;
		int blockAlign = 0;
		int dataPos = -1, dataLen = 0;

		if(b.Length < 12 || Encoding.ASCII.GetString(b, 0, 4) != "RIFF" || Encoding.ASCII.GetString(b, 8, 4) != "WAVE")
			throw new Exception("Not a RIFF WAVE file: " + path);

		int pos = 12;
		while(pos + 8 <= b.Length)
		{
			string id = Encoding.ASCII.GetString(b, pos, 4);
			int len = BitConverter.ToInt32(b, pos + 4);
			int body = pos + 8;

			if(len < 0 || body + len > b.Length)
				len = b.Length - body;

			if(id == "fmt ")
			{
				format = BitConverter.ToUInt16(b, body);
				channels = BitConverter.ToUInt16(b, body + 2);
				rate = BitConverter.ToInt32(b, body + 4);
				blockAlign = BitConverter.ToUInt16(b, body + 12);
				bits = BitConverter.ToUInt16(b, body + 14);

				/* WAVE_FORMAT_EXTENSIBLE: the real tag opens the sub-format GUID */
				if(format == 0xFFFE && len >= 26)
					format = BitConverter.ToUInt16(b, body + 24);
			}
			else if(id == "data")
			{
				dataPos = body;
				dataLen = len;
			}

			pos = body + len + (len & 1);
		}

		if(channels == 0)
			throw new Exception("No fmt chunk in " + path);
		if(dataPos < 0)
			throw new Exception("No data chunk in " + path);
		if(format == 1 && bits != 8 && bits != 16 && bits != 24 && bits != 32)
			throw new Exception("Unsupported PCM sample size: " + bits + " bits");
		if(format == 3 && bits != 32 && bits != 64)
			throw new Exception("Unsupported float sample size: " + bits + " bits");
		if(format != 1 && format != 3)
			throw new Exception("Unsupported WAV format tag " + format + " (PCM or float only)");

		int bytesPer = bits / 8;
		if(blockAlign < channels * bytesPer)
			blockAlign = channels * bytesPer;

		int frames = dataLen / blockAlign;
		double[] mono = new double[frames];

		for(int i = 0; i < frames; i++)
		{
			double sum = 0;
			for(int c = 0; c < channels; c++)
				sum += ReadSample(b, dataPos + i * blockAlign + c * bytesPer, format, bits);
			mono[i] = sum / channels;
		}

		return mono;
	}

	static double ReadSample(byte[] b, int p, int format, int bits)
	{
		if(format == 3)
			return bits == 32 ? BitConverter.ToSingle(b, p) : BitConverter.ToDouble(b, p);

		switch(bits)
		{
		case 8:
			return (b[p] - 128) / 128.0;
		case 16:
			return BitConverter.ToInt16(b, p) / 32768.0;
		case 24:
			return ((b[p] << 8 | b[p + 1] << 16 | b[p + 2] << 24) >> 8) / 8388608.0;
		default:
			return BitConverter.ToInt32(b, p) / 2147483648.0;
		}
	}

	/* Band-limited resampling: each output sample is the input convolved with
	   a Blackman-windowed sinc centred on its exact position. The cutoff sits
	   at 92.5% of the lower Nyquist rate and the kernel spans 64 zero crossings
	   each side, so the transition band (~5.5 * cutoff / 64, ~320Hz going to
	   8kHz) ends below 4kHz and nothing aliases. */
	public static double[] Resample(double[] x, int inRate)
	{
		if(inRate == OutRate)
			return (double[])x.Clone();

		const int ZeroCrossings = 64;
		double cutoff = 0.925 * Math.Min(inRate, OutRate) / 2.0;
		double scale = 2.0 * cutoff / inRate;
		double half = ZeroCrossings / scale;
		long outLen = (long)Math.Floor((double)x.Length * OutRate / inRate);
		double[] y = new double[outLen];

		for(long j = 0; j < outLen; j++)
		{
			double t = (double)j * inRate / OutRate;
			int lo = Math.Max(0, (int)Math.Ceiling(t - half));
			int hi = Math.Min(x.Length - 1, (int)Math.Floor(t + half));
			double acc = 0;

			for(int n = lo; n <= hi; n++)
			{
				double d = t - n;
				double a = Math.PI * d / half;
				double w = 0.42 + 0.5 * Math.Cos(a) + 0.08 * Math.Cos(2.0 * a);
				double s = scale * d;
				double sinc = s == 0 ? 1.0 : Math.Sin(Math.PI * s) / (Math.PI * s);
				acc += x[n] * scale * sinc * w;
			}

			y[j] = acc;
		}

		return y;
	}

	/* PLIB Reference, "The A-Law encoding scheme": x is 13-bit two's
	   complement (-4095..4095). The result's top bit is set for negative
	   samples, which is the reverse of most G.711 A-law code. */
	public static byte Encode(int x)
	{
		int p = 0x80, s, y;

		if(x < 0)
		{
			x = -x;
			p = 0;
		}

		if(x > 4095)
			x = 4095;

		for(s = 7; s > 0; s--)
			if((x & (1 << (s + 4))) != 0)
				break;

		if(s == 0)
			y = x >> 1;
		else
			y = ((x >> s) & 0x0F) | (s << 4);

		return (byte)((y | p) ^ 0xD5);
	}

	public static byte[] Convert(double[] y, double level, out int peak, out int clipped)
	{
		byte[] file = new byte[32 + y.Length];
		peak = 0;
		clipped = 0;

		for(int i = 0; i < y.Length; i++)
		{
			int v = (int)Math.Round(y[i] * level);

			if(v > 4095 || v < -4095)
			{
				clipped++;
				v = v > 0 ? 4095 : -4095;
			}

			if(Math.Abs(v) > peak)
				peak = Math.Abs(v);

			file[32 + i] = Encode(v);
		}

		return file;
	}

	public static void WriteHeader(byte[] file, int samples, int silence, int repeats)
	{
		byte[] sig = Encoding.ASCII.GetBytes("ALawSoundFile**");
		Array.Copy(sig, 0, file, 0, sig.Length);
		file[15] = 0;
		Array.Copy(BitConverter.GetBytes((ushort)0x100F), 0, file, 16, 2);
		Array.Copy(BitConverter.GetBytes((uint)samples), 0, file, 18, 4);
		Array.Copy(BitConverter.GetBytes((ushort)silence), 0, file, 22, 2);
		Array.Copy(BitConverter.GetBytes((ushort)repeats), 0, file, 24, 2);
	}
}
"@

$InputPath = ""
$OutputPath = ""
$Gain = 100.0
$Normalize = $false
$Silence = 0
$Repeats = 1

$allArgs = $args

for($argIndex = 0; $argIndex -lt $allArgs.Count; $argIndex++)
{
	$arg = $allArgs[$argIndex]

	if($arg -eq "-Normalize")
	{
		$Normalize = $true
		continue
	}

	if($arg -eq "-OutputPath" -or $arg -eq "-Gain" -or $arg -eq "-Silence" -or $arg -eq "-Repeats")
	{
		$argIndex++

		if($argIndex -ge $allArgs.Count)
		{
			throw "$arg requires a value."
		}

		$value = $allArgs[$argIndex]

		switch($arg)
		{
			"-OutputPath" { $OutputPath = $value }
			"-Gain" { $Gain = [double]$value }
			"-Silence" { $Silence = [int]$value }
			"-Repeats" { $Repeats = [int]$value }
		}

		continue
	}

	if($InputPath -eq "")
	{
		$InputPath = $arg
	}
	elseif($OutputPath -eq "")
	{
		$OutputPath = $arg
	}
	else
	{
		throw "Unexpected argument: $arg"
	}
}

if($InputPath -eq "")
{
	Write-Host "Usage: convert_sound <in.wav> [<out.wve> | -OutputPath <out.wve>]"
	Write-Host "                     [-Gain <percent>] [-Normalize] [-Silence <ticks>] [-Repeats <n>]"
	Write-Host ""
	Write-Host "  -Gain 100 (default) matches the SDK's wav2wve: 16-bit full scale -> +-2048."
	Write-Host "  -Gain 200 uses the codec's full 13-bit range. -Normalize sets the peak to 4095."
	Write-Host "  -Silence is 1/32s ticks appended to each repeat on playback."
	exit 1
}

if($Silence -lt 0 -or $Silence -gt 65535 -or $Repeats -lt 0 -or $Repeats -gt 65535)
{
	throw "-Silence and -Repeats must be 0..65535."
}

$InputPath = [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $InputPath))

if($OutputPath -eq "")
{
	$OutputPath = [System.IO.Path]::ChangeExtension($InputPath, ".wve")
}
else
{
	$OutputPath = [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $OutputPath))
}

$rate = 0
$channels = 0
$bits = 0
$format = 0
$mono = [WveConverter]::ReadWav($InputPath, [ref]$rate, [ref]$channels, [ref]$bits, [ref]$format)
$samples = [WveConverter]::Resample($mono, $rate)

if($Normalize)
{
	$max = 0.0

	foreach($s in $samples)
	{
		if([Math]::Abs($s) -gt $max)
		{
			$max = [Math]::Abs($s)
		}
	}

	if($max -eq 0.0)
	{
		$level = 0.0
	}
	else
	{
		$level = 4095.0 / $max
	}
}
else
{
	$level = 2048.0 * $Gain / 100.0
}

$peak = 0
$clipped = 0
$file = [WveConverter]::Convert($samples, $level, [ref]$peak, [ref]$clipped)
[WveConverter]::WriteHeader($file, $samples.Length, $Silence, $Repeats)
[System.IO.File]::WriteAllBytes($OutputPath, $file)

$kind = if($format -eq 3) { "float" } else { "PCM" }
$ms = [int]($samples.Length * 1000 / 8000)
Write-Host ("{0}: {1}Hz {2}-bit {3} x{4}, {5} samples" -f [System.IO.Path]::GetFileName($InputPath), $rate, $bits, $kind, $channels, $mono.Length)
Write-Host ("  -> {0}: {1} samples, {2}ms, {3} bytes, peak {4}/4095{5}" -f [System.IO.Path]::GetFileName($OutputPath), $samples.Length, $ms, $file.Length, $peak, $(if($clipped) { ", $clipped clipped" } else { "" }))
