@echo off
cd /d "%~dp0"
call app\gen_sln.bat
if errorlevel 1 exit /b 1
start "" "D:\chengmao\imagedetect\app\imagedetect.sln"
