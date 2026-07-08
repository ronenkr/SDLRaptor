@echo off
setlocal

set "VSWHERE_DIR=C:\Program Files (x86)\Microsoft Visual Studio\Installer"
set "VSDEVCMD=C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat"
set "CMAKE=C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set "NINJA_DIR=C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja"

if not exist "%VSDEVCMD%" (
    echo Could not find VsDevCmd.bat at "%VSDEVCMD%"
    exit /b 1
)
if not exist "%CMAKE%" (
    echo Could not find cmake.exe at "%CMAKE%"
    exit /b 1
)

REM vswhere.exe must be on PATH before VsDevCmd.bat runs, or it fails to locate the MSVC toolset.
set "PATH=%VSWHERE_DIR%;%PATH%"

REM Must use "call" - without it, control never returns to run the rest of this script.
call "%VSDEVCMD%" -arch=x64
if errorlevel 1 exit /b 1

set "PATH=%NINJA_DIR%;%PATH%"
set "BUILD_DIR=%~dp0build"
set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=RelWithDebInfo"

set "SDL3_ARG="
if not "%SDL3_ROOT%"=="" set "SDL3_ARG=-DCMAKE_PREFIX_PATH=%SDL3_ROOT%"

"%CMAKE%" -S "%~dp0." -B "%BUILD_DIR%" -G Ninja -DCMAKE_BUILD_TYPE=%CONFIG% %SDL3_ARG%
if errorlevel 1 exit /b 1

"%CMAKE%" --build "%BUILD_DIR%"
if errorlevel 1 exit /b 1

echo.
echo Build succeeded: %BUILD_DIR%\raptor.exe
endlocal
