@echo off
setlocal

set "VCVARSALL=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat"
set "CMAKE=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set "NINJA_DIR=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja"

if not exist "%VCVARSALL%" (
    echo Could not find vcvarsall.bat at "%VCVARSALL%"
    exit /b 1
)
if not exist "%CMAKE%" (
    echo Could not find cmake.exe at "%CMAKE%"
    exit /b 1
)

set "PATH=%NINJA_DIR%;%PATH%"
set "BUILD_DIR=%~dp0build"
set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=RelWithDebInfo"

call "%VCVARSALL%" x64
if errorlevel 1 exit /b 1

"%CMAKE%" -S "%~dp0." -B "%BUILD_DIR%" -G Ninja -DCMAKE_BUILD_TYPE=%CONFIG%
if errorlevel 1 exit /b 1

"%CMAKE%" --build "%BUILD_DIR%"
if errorlevel 1 exit /b 1

echo.
echo Build succeeded: %BUILD_DIR%\raptor.exe
endlocal
