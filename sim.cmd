@echo off
rem Wrapper care merge din Command Prompt (cmd) si din PowerShell.
rem Exemple:  sim up   |   sim logs   |   sim down   |   sim test
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0sim.ps1" %*
exit /b %ERRORLEVEL%
