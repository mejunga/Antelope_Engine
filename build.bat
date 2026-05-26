@echo off
setlocal EnableDelayedExpansion

set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"

set "BUILD=%ROOT%\build"
set "EXE=%BUILD%\bin\Release\AntelopeEngine.exe"
set "SANDBOX=%ROOT%\Sandbox"

if not defined VCPKG_ROOT (
    echo [Antelope] VCPKG_ROOT is not set. Add it to your environment variables.
    exit /b 1
)

echo [Antelope] Configuring...
cmake -S "%ROOT%" -B "%BUILD%" ^
    -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake" ^
    -DVCPKG_TARGET_TRIPLET=x64-windows
if %ERRORLEVEL% neq 0 ( echo [Antelope] Configure failed. & exit /b 1 )

echo [Antelope] Building Release...
cmake --build "%BUILD%" --config Release --parallel
if %ERRORLEVEL% neq 0 ( echo [Antelope] Build failed. & exit /b 1 )

echo.
echo [Antelope] Build complete.
echo   Output : %EXE%
echo.

start "" /D "%BUILD%\bin\Release" "%EXE%" "%SANDBOX%"