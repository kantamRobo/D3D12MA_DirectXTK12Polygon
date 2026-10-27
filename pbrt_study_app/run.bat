@echo off
setlocal enabledelayedexpansion

echo ======================================================
echo   PBRT-v4 Practice ^& Review App (Chapters 2 - 10)
echo   Initializing startup sequence...
echo ======================================================

cd /d "%~dp0"

where cargo >nul 2>nul
if %errorlevel% neq 0 (
    echo [ERROR] 'cargo' not found in PATH.
    pause
    exit /b 1
)

netstat -ano | findstr ":8040" | findstr "LISTENING" >nul 2>nul
if %errorlevel% equ 0 (
    echo [INFO] Server is already running on http://localhost:8040
    start http://localhost:8040
    ping 127.0.0.1 -n 2 >nul
    exit /b 0
)

if not exist "target\debug\pbrt_study_app.exe" (
    echo [BUILD] Building binary for the first time...
    cargo build
    if %errorlevel% neq 0 (
        echo [ERROR] Build failed.
        pause
        exit /b 1
    )
)

start "" cmd /c "timeout /t 1 >nul & start http://localhost:8040"
echo [START] Starting server on http://localhost:8040 ...
echo [HINT] Press Ctrl+C in this window to stop the server.
echo.
target\debug\pbrt_study_app.exe
pause
