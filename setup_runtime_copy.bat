@echo off
REM Called by VS PostBuildEvent with OutDir as %1
setlocal EnableExtensions
set "ARG=%~1"
if "%ARG%"=="" set "ARG=%~dp0app\release"
REM Strip trailing dot (quote-safe) and backslash
if "%ARG:~-1%"=="." set "ARG=%ARG:~0,-1%"
if "%ARG:~-1%"=="\" set "ARG=%ARG:~0,-1%"
if "%ARG:~-1%"=="/" set "ARG=%ARG:~0,-1%"

cd /d "%~dp0"

REM Absolute path (X:...) vs relative (release)
if "%ARG:~1,1%"==":" (
  set "OUTDIR=%ARG%"
) else (
  set "OUTDIR=%~dp0app\%ARG%"
)

if not exist "%OUTDIR%" mkdir "%OUTDIR%"

if not exist "third_party\snap7\win64\snap7.dll" call third_party\build_stubs.bat
if not exist "third_party\onna\bin\OnnaCamera.dll" call third_party\build_stubs.bat

set "MVS_RT=C:\Program Files (x86)\Common Files\MVS\Runtime\Win64_x64"
set "PYROOT=C:\Users\rkx\AppData\Local\Python\pythoncore-3.14-64"
set "OPENCV_BIN=D:\opencv\opencv\build\x64\vc16\bin"

copy /Y "third_party\snap7\win64\snap7.dll" "%OUTDIR%\" >nul 2>nul
copy /Y "third_party\onna\bin\OnnaCamera.dll" "%OUTDIR%\" >nul 2>nul
if exist "%MVS_RT%\MvCameraControl.dll" copy /Y "%MVS_RT%\MvCameraControl.dll" "%OUTDIR%\" >nul 2>nul
if exist "%MVS_RT%\FormatConversion.dll" copy /Y "%MVS_RT%\FormatConversion.dll" "%OUTDIR%\" >nul 2>nul
if exist "%MVS_RT%\MediaProcess.dll" copy /Y "%MVS_RT%\MediaProcess.dll" "%OUTDIR%\" >nul 2>nul
if exist "%MVS_RT%\GCBase_MD_VC120_v3_0_MVS_v3_1_0.dll" copy /Y "%MVS_RT%\GCBase_MD_VC120_v3_0_MVS_v3_1_0.dll" "%OUTDIR%\" >nul 2>nul
if exist "%MVS_RT%\GenApi_MD_VC120_v3_0_MVS_v3_1_0.dll" copy /Y "%MVS_RT%\GenApi_MD_VC120_v3_0_MVS_v3_1_0.dll" "%OUTDIR%\" >nul 2>nul
if exist "%MVS_RT%\Log_MD_VC120_v3_0_MVS_v3_1_0.dll" copy /Y "%MVS_RT%\Log_MD_VC120_v3_0_MVS_v3_1_0.dll" "%OUTDIR%\" >nul 2>nul
if exist "%MVS_RT%\log4cpp_MD_VC120_v3_0_MVS_v3_1_0.dll" copy /Y "%MVS_RT%\log4cpp_MD_VC120_v3_0_MVS_v3_1_0.dll" "%OUTDIR%\" >nul 2>nul
if exist "%MVS_RT%\MathParser_MD_VC120_v3_0_MVS_v3_1_0.dll" copy /Y "%MVS_RT%\MathParser_MD_VC120_v3_0_MVS_v3_1_0.dll" "%OUTDIR%\" >nul 2>nul
if exist "%MVS_RT%\NodeMapData_MD_VC120_v3_0_MVS_v3_1_0.dll" copy /Y "%MVS_RT%\NodeMapData_MD_VC120_v3_0_MVS_v3_1_0.dll" "%OUTDIR%\" >nul 2>nul
if exist "%MVS_RT%\XmlParser_MD_VC120_v3_0_MVS_v3_1_0.dll" copy /Y "%MVS_RT%\XmlParser_MD_VC120_v3_0_MVS_v3_1_0.dll" "%OUTDIR%\" >nul 2>nul
if exist "%MVS_RT%\libmmd.dll" copy /Y "%MVS_RT%\libmmd.dll" "%OUTDIR%\" >nul 2>nul
if exist "%OPENCV_BIN%\opencv_world4100.dll" copy /Y "%OPENCV_BIN%\opencv_world4100.dll" "%OUTDIR%\" >nul 2>nul
if exist "%OPENCV_BIN%\opencv_world4100d.dll" copy /Y "%OPENCV_BIN%\opencv_world4100d.dll" "%OUTDIR%\" >nul 2>nul
if exist "%PYROOT%\python314.dll" copy /Y "%PYROOT%\python314.dll" "%OUTDIR%\" >nul 2>nul
if exist "%PYROOT%\python3.dll" copy /Y "%PYROOT%\python3.dll" "%OUTDIR%\" >nul 2>nul
echo Runtime DLLs staged to %OUTDIR%
endlocal
exit /b 0
