$repo = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$py = "C:\Users\rkx\AppData\Local\Python\pythoncore-3.14-64"
$mvsRt = "C:\Program Files (x86)\Common Files\MVS\Runtime\Win64_x64"
$qtBin = "D:\Qt\Qt5.14.2\5.14.2\msvc2017_64\bin"
$opencvBin = "D:\opencv\opencv\build\x64\vc16\bin"
$snap = Join-Path $repo "third_party\snap7\win64"
$onna = Join-Path $repo "third_party\onna\bin"

$path = "PATH=$($PSScriptRoot)\release;$($PSScriptRoot)\debug;$mvsRt;$py;$qtBin;$opencvBin;%PATH%"
$envBlock = @(
    $path
    "PYTHONHOME=$py"
    "PYTHONPATH=$repo;$py;$py\Lib;$py\DLLs"
    "SNAP7_DIR=$snap"
    "ONNA_SDK_DIR=$onna"
    "MVCAM_COMMON_RUNENV=C:\Program Files (x86)\MVS\Development"
    "OPENCV_DIR=D:\opencv\opencv\build"
) -join "`n"

$userPath = Join-Path $PSScriptRoot "imagedetect.vcxproj.user"
@"
<?xml version="1.0" encoding="utf-8"?>
<Project ToolsVersion="Current" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup Condition="'`$(Configuration)|`$(Platform)'=='Release|x64'">
    <LocalDebuggerWorkingDirectory>`$(ProjectDir)..\</LocalDebuggerWorkingDirectory>
    <LocalDebuggerEnvironment>$envBlock</LocalDebuggerEnvironment>
    <DebuggerFlavor>WindowsLocalDebugger</DebuggerFlavor>
  </PropertyGroup>
  <PropertyGroup Condition="'`$(Configuration)|`$(Platform)'=='Debug|x64'">
    <LocalDebuggerWorkingDirectory>`$(ProjectDir)..\</LocalDebuggerWorkingDirectory>
    <LocalDebuggerEnvironment>$envBlock</LocalDebuggerEnvironment>
    <DebuggerFlavor>WindowsLocalDebugger</DebuggerFlavor>
  </PropertyGroup>
</Project>
"@ | Set-Content -Path $userPath -Encoding UTF8
Write-Host "Wrote $userPath"
