@echo off
setlocal enabledelayedexpansion

cd /d "%~dp0"

echo ===================================================
echo   PerformanceViewer Auto Build and Copy Script
echo ===================================================

rem Find MSBuild using vswhere
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo [ERROR] vswhere.exe was not found. Please install Visual Studio.
    pause
    exit /b 1
)

for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do (
    set "MSBUILD_PATH=%%i"
)

if not exist "!MSBUILD_PATH!" (
    echo [ERROR] MSBuild.exe was not found.
    pause
    exit /b 1
)

echo [INFO] Found MSBuild: !MSBUILD_PATH!

rem Ask build configuration (default Debug)
set "CONFIG=Debug"
echo Choose build configuration:
echo [1] Debug (Default)
echo [2] Release
set /p CHOICE="Enter choice (1 or 2): "
if "%CHOICE%"=="2" (
    set "CONFIG=Release"
)

echo [INFO] Building configuration: %CONFIG%

rem Run MSBuild on project file
"!MSBUILD_PATH!" PerformanceViewer.vcxproj -t:Build -p:Configuration=%CONFIG% -p:Platform=x64
if %ERRORLEVEL% neq 0 (
    echo [ERROR] MSBuild failed with error code %ERRORLEVEL%.
    pause
    exit /b %ERRORLEVEL%
)

echo [SUCCESS] Build completed successfully.
pause
