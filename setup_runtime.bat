@echo off
setlocal
cd /d "%~dp0"

echo === Build stub SDKs (Snap7 + Onna) ===
call third_party\build_stubs.bat
if errorlevel 1 (
  echo Stub build failed
  exit /b 1
)

set RELEASE=%~dp0app\release
if not exist "%RELEASE%\imagedetect.exe" (
  echo Building app...
  call app\build.bat
  if errorlevel 1 exit /b 1
)

echo === Stage runtime DLLs into app\release ===
set MVS_RT=C:\Program Files (x86)\Common Files\MVS\Runtime\Win64_x64
set PYROOT=C:\Users\rkx\AppData\Local\Python\pythoncore-3.14-64
set OPENCV_BIN=D:\opencv\opencv\build\x64\vc16\bin

copy /Y "%~dp0third_party\snap7\win64\snap7.dll" "%RELEASE%\" >nul
copy /Y "%~dp0third_party\onna\bin\OnnaCamera.dll" "%RELEASE%\" >nul

if exist "%MVS_RT%\MvCameraControl.dll" (
  copy /Y "%MVS_RT%\MvCameraControl.dll" "%RELEASE%\" >nul
  REM GenApi / FormatConversion often required beside MvCameraControl
  copy /Y "%MVS_RT%\FormatConversion.dll" "%RELEASE%\" >nul 2>nul
  copy /Y "%MVS_RT%\MediaProcess.dll" "%RELEASE%\" >nul 2>nul
  copy /Y "%MVS_RT%\GCBase_MD_VC120_v3_0_MVS_v3_1_0.dll" "%RELEASE%\" >nul 2>nul
  copy /Y "%MVS_RT%\GenApi_MD_VC120_v3_0_MVS_v3_1_0.dll" "%RELEASE%\" >nul 2>nul
  copy /Y "%MVS_RT%\Log_MD_VC120_v3_0_MVS_v3_1_0.dll" "%RELEASE%\" >nul 2>nul
  copy /Y "%MVS_RT%\log4cpp_MD_VC120_v3_0_MVS_v3_1_0.dll" "%RELEASE%\" >nul 2>nul
  copy /Y "%MVS_RT%\MathParser_MD_VC120_v3_0_MVS_v3_1_0.dll" "%RELEASE%\" >nul 2>nul
  copy /Y "%MVS_RT%\NodeMapData_MD_VC120_v3_0_MVS_v3_1_0.dll" "%RELEASE%\" >nul 2>nul
  copy /Y "%MVS_RT%\XmlParser_MD_VC120_v3_0_MVS_v3_1_0.dll" "%RELEASE%\" >nul 2>nul
  copy /Y "%MVS_RT%\libmmd.dll" "%RELEASE%\" >nul 2>nul
)

if exist "%OPENCV_BIN%\opencv_world4100.dll" copy /Y "%OPENCV_BIN%\opencv_world4100.dll" "%RELEASE%\" >nul
if exist "%PYROOT%\python314.dll" copy /Y "%PYROOT%\python314.dll" "%RELEASE%\" >nul
if exist "%PYROOT%\python3.dll" copy /Y "%PYROOT%\python3.dll" "%RELEASE%\" >nul

echo.
echo Runtime staged. Key files:
dir /b "%RELEASE%\imagedetect.exe" "%RELEASE%\snap7.dll" "%RELEASE%\OnnaCamera.dll" "%RELEASE%\MvCameraControl.dll" "%RELEASE%\python314.dll" "%RELEASE%\opencv_world4100.dll" 2>nul
echo.
echo Done. Use run.bat to start.
endlocal
