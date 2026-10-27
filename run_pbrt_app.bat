@echo off
setlocal enabledelayedexpansion

echo ======================================================
echo   PBRT-v4 Practice ^& Review App (Chapters 2 - 10)
echo   Initializing startup sequence...
echo ======================================================

:: Navigate to app directory
cd /d "%~dp0pbrt_study_app"
if not exist "Cargo.toml" (
    cd /d "%~dp0"
    if not exist "Cargo.toml" (
        echo [ERROR] Could not locate pbrt_study_app directory.
        pause
        exit /b 1
    )
)

:: Verify Rust toolchain
where cargo >nul 2>nul
if %errorlevel% neq 0 (
    echo [ERROR] 'cargo' not found in PATH.
    echo Please install Rust or ensure cargo is in your PATH.
    pause
    exit /b 1
)

:: Check if port 8040 is already running
netstat -ano | findstr ":8040" | findstr "LISTENING" >nul 2>nul
if %errorlevel% equ 0 (
    echo [INFO] Server is already running on http://localhost:8040
    echo [INFO] Opening default browser...
    start http://localhost:8040
    echo Application launched.
    ping 127.0.0.1 -n 2 >nul
    exit /b 0
)

:: Check binary build
if not exist "target\debug\pbrt_study_app.exe" (
    echo [BUILD] Building binary for the first time...
    cargo build
    if %errorlevel% neq 0 (
        echo [ERROR] Build failed.
        pause
        exit /b 1
    )
)

:: Launch browser in 1 second
echo [LAUNCH] Preparing to open browser...
start "" cmd /c "timeout /t 1 >nul & start http://localhost:8040"

:: Start the server
echo [START] Starting server on http://localhost:8040 ...
echo [HINT] Press Ctrl+C in this window to stop the server.
echo.
target\debug\pbrt_study_app.exe

pause
