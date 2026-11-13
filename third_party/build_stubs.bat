@echo off
setlocal
call "D:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" || exit /b 1

cd /d "%~dp0"

echo Building snap7.dll stub...
mkdir snap7\win64 2>nul
cl /nologo /O2 /LD snap7\stub\snap7_stub.c /link /OUT:snap7\win64\snap7.dll
if errorlevel 1 exit /b 1
del /q snap7_stub.obj 2>nul
del /q snap7\win64\snap7.exp 2>nul
del /q snap7\win64\snap7.lib 2>nul
move /y snap7.lib snap7\win64\ >nul 2>nul
move /y snap7.exp snap7\win64\ >nul 2>nul

echo Building OnnaCamera.dll stub...
mkdir onna\bin 2>nul
cl /nologo /O2 /LD /Ionna\include onna\stub\onna_stub.c /link /OUT:onna\bin\OnnaCamera.dll
if errorlevel 1 exit /b 1
del /q onna_stub.obj 2>nul
move /y OnnaCamera.lib onna\bin\ >nul 2>nul
move /y OnnaCamera.exp onna\bin\ >nul 2>nul

echo Stub DLLs ready:
dir /b snap7\win64\snap7.dll
dir /b onna\bin\OnnaCamera.dll
endlocal
