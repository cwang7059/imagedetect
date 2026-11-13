@echo off
setlocal
cd /d "%~dp0"

set RELEASE=%~dp0app\release
set MVS_RT=C:\Program Files (x86)\Common Files\MVS\Runtime\Win64_x64
set PYROOT=C:\Users\rkx\AppData\Local\Python\pythoncore-3.14-64
set QTBIN=D:\Qt\Qt5.14.2\5.14.2\msvc2017_64\bin
set OPENCV_BIN=D:\opencv\opencv\build\x64\vc16\bin

if not exist "%RELEASE%\imagedetect.exe" (
  echo imagedetect.exe missing — run setup_runtime.bat first
  exit /b 1
)
if not exist "%RELEASE%\snap7.dll" (
  echo Runtime incomplete — run setup_runtime.bat first
  exit /b 1
)

REM Python embed needs home + path for encodings / site-packages
set PYTHONHOME=%PYROOT%
set PYTHONPATH=%~dp0;%PYROOT%;%PYROOT%\Lib;%PYROOT%\DLLs
set PATH=%RELEASE%;%MVS_RT%;%PYROOT%;%PYROOT%\Scripts;%QTBIN%;%OPENCV_BIN%;%PATH%
set SNAP7_DIR=%~dp0third_party\snap7\win64
set ONNA_SDK_DIR=%~dp0third_party\onna\bin
set MVCAM_COMMON_RUNENV=C:\Program Files (x86)\MVS\Development

echo Starting imagedetect...
echo   PYTHONHOME=%PYTHONHOME%
echo   PYTHONPATH=%PYTHONPATH%
start "" /D "%RELEASE%" "%RELEASE%\imagedetect.exe"
endlocal
