@echo off
setlocal
set "spriteInput=%~dp0..\sprites"
set "spriteOutput=%~dp0..\spr"

if not exist "%spriteInput%\*.png" (
	echo No PNG sprites found in "%spriteInput%".
	exit /b 1
)

if not exist "%spriteOutput%\" (
	mkdir "%spriteOutput%"
	if errorlevel 1 exit /b 1
)

for %%F in ("%spriteInput%\*.png") do (
	echo Converting %%~nxF...
	call "%~dp0convert_sprite.bat" /f "%%~fF" "%spriteOutput%\%%~nF.spr"
	if errorlevel 1 (
		echo Failed to convert %%~nxF.
		exit /b 1
	)
)

echo All sprites converted successfully.
exit /b 0
