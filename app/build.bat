@echo off
setlocal
call "D:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" || exit /b 1
set "PATH=D:\Qt\Qt5.14.2\5.14.2\msvc2017_64\bin;%PATH%"
set "OPENCV_DIR=D:\opencv\opencv\build"
set "PATH=%OPENCV_DIR%\x64\vc16\bin;%PATH%"
set "MVS_DIR=C:\Program Files (x86)\MVS\Development"
set "MVS_RUNTIME=C:\Program Files (x86)\Common Files\MVS\Runtime\Win64_x64"
cd /d "%~dp0"
where cl
where qmake
qmake imagedetect.pro
if errorlevel 1 exit /b 1
nmake
if errorlevel 1 exit /b 1

if exist release\imagedetect.exe (
  copy /Y "%OPENCV_DIR%\x64\vc16\bin\opencv_world4100.dll" release\ >nul
  copy /Y "%OPENCV_DIR%\x64\vc16\bin\opencv_videoio_ffmpeg4100_64.dll" release\ >nul 2>nul
  if exist "%MVS_RUNTIME%\MvCameraControl.dll" copy /Y "%MVS_RUNTIME%\MvCameraControl.dll" release\ >nul
  if exist "..\third_party\snap7\win64\snap7.dll" copy /Y "..\third_party\snap7\win64\snap7.dll" release\ >nul
  if exist "..\third_party\onna\bin\OnnaCamera.dll" copy /Y "..\third_party\onna\bin\OnnaCamera.dll" release\ >nul
)
echo.
echo BUILD OK
dir /b release\imagedetect.exe 2>nul || dir /b debug\imagedetect.exe 2>nul
echo OpenCV: %OPENCV_DIR%
echo MVS_DIR: %MVS_DIR%
endlocal
