@echo off
setlocal EnableDelayedExpansion

cd /d "%~dp0"

set "PRESET=%~1"
if "%PRESET%"=="" set "PRESET=windows-msvc-x86-release"

set "ARCH=x86"
echo %PRESET%| findstr /i "x64" >nul 2>nul
if not errorlevel 1 set "ARCH=x64"

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"

set "VSDEVCMD="
if exist "%VSWHERE%" for /f "usebackq delims=" %%i in (`call "%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do if not defined VSDEVCMD if exist "%%i\Common7\Tools\VsDevCmd.bat" set "VSDEVCMD=%%i\Common7\Tools\VsDevCmd.bat"

if not defined VSDEVCMD (
    echo VsDevCmd.bat not found. Install Visual Studio with the C++ x86/x64 toolset.
    exit /b 1
)

where cl.exe >nul 2>nul
if errorlevel 1 (
    call "%VSDEVCMD%" -arch=%ARCH% -host_arch=x64
    if errorlevel 1 (
        echo Failed to initialize the Visual Studio environment.
        exit /b 1
    )
)

cmake --preset %PRESET%
if errorlevel 1 (
    echo CMake configure failed.
    exit /b 1
)

cmake --build --preset %PRESET%
if errorlevel 1 (
    echo Build failed.
    exit /b 1
)

exit /b 0