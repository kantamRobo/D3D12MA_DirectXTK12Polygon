@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"
title 第二種電気工事士 配線図マスター 起動ランチャー

rem ========================================================
rem  第二種電気工事士 配線図マスター 起動シークエンス v2.0
rem  湯川開発チーム: デバッグマン ＆ コーダーマン協働改修
rem ========================================================

set "TARGET_HTML=%~dp0denko2_wiring_app.html"

echo ========================================================
echo   第二種電気工事士 配線図マスター (即時復習＆回路描画)
echo ========================================================
echo.

rem [Step 1] ファイル存在確認
if not exist "%TARGET_HTML%" (
    echo [エラー] 起動対象のHTMLファイルが見つかりません。
    echo パス: "%TARGET_HTML%"
    echo.
    echo フォルダ内に denko2_wiring_app.html が存在するか確認してください。
    echo.
    pause
    exit /b 1
)

echo [1/2] アプリケーションファイルを検出しました。
echo       %TARGET_HTML%
echo.

rem [Step 2] ブラウザ起動（多重フォールバック方式）
echo [2/2] 既定のWebブラウザで起動しています...

rem 方式A: explorer.exe経由（Windows標準シェルによる最も安全な関連付け起動）
explorer.exe "%TARGET_HTML%"

if !ERRORLEVEL! equ 0 goto LAUNCH_SUCCESS

rem 方式B: rundll32 url.dll経由（フォールバック）
rundll32.exe url.dll,FileProtocolHandler "%TARGET_HTML%"

if !ERRORLEVEL! equ 0 goto LAUNCH_SUCCESS

rem 方式C: start コマンド（最終フォールバック）
start "" "%TARGET_HTML%"

:LAUNCH_SUCCESS
echo.
echo ========================================================
echo  [成功] ブラウザでアプリを正常に立ち上げました！
echo  配線図・回路描画の学習を開始してください。
echo ========================================================
timeout /t 1 > nul
exit /b 0
