@echo off
setlocal EnableDelayedExpansion
set "spriteInput=%~dp0..\sprites"
set "spriteOutput=%~dp0..\spr"

rem One .spr per sprite: sprites\<base>0.png .. <base>7.png become spr\<base>.spr.
rem Frame numbers are single digits, so the base is the name of frame 0 less
rem its last character (m2490.png is base m249, frame 0).

if not exist "%spriteInput%\*0.png" (
	echo No PNG sprites found in "%spriteInput%".
	exit /b 1
)

if not exist "%spriteOutput%\" (
	mkdir "%spriteOutput%"
	if errorlevel 1 exit /b 1
)

for %%F in ("%spriteInput%\*0.png") do (
	set "frameName=%%~nF"
	set "base=!frameName:~0,-1!"
	echo Converting !base!...
	call "%~dp0convert_sprite.bat" /f "%spriteInput%\!base!" "%spriteOutput%\!base!.spr"
	if errorlevel 1 (
		echo Failed to convert !base!.
		exit /b 1
	)
)

echo All sprites converted successfully.
exit /b 0
