@echo off
setlocal
call "D:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" || exit /b 1
set "PATH=D:\Qt\Qt5.14.2\5.14.2\msvc2017_64\bin;%PATH%"
set "OPENCV_DIR=D:\opencv\opencv\build"
set "MVS_DIR=C:\Program Files (x86)\MVS\Development"
cd /d "%~dp0"

echo Generating Visual Studio project from imagedetect.pro ...
qmake -tp vc imagedetect.pro
if errorlevel 1 exit /b 1

powershell -NoProfile -Command "(Get-Content 'imagedetect.vcxproj' -Raw) -replace '<PlatformToolset>v14\d</PlatformToolset>','<PlatformToolset>v145</PlatformToolset>' | Set-Content 'imagedetect.vcxproj' -NoNewline"

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0patch_vcxproj_postbuild.ps1"
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0write_sln.ps1"
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0write_vcxproj_user.ps1"
if errorlevel 1 exit /b 1

echo.
echo SLN ready: %cd%\imagedetect.sln
echo Config: Release^|x64 or Debug^|x64, then F5.
endlocal
