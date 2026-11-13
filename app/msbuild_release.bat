@echo off
call "D:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" || exit /b 1
cd /d "%~dp0"
msbuild imagedetect.sln /p:Configuration=Release /p:Platform=x64 /m /v:minimal
exit /b %ERRORLEVEL%
