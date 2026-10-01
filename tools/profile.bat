@echo off
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0profile.ps1" %*
exit /b %ERRORLEVEL%
