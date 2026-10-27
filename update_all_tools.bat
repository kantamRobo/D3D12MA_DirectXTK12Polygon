@echo off
setlocal enabledelayedexpansion
chcp 65001 >nul
title Yukawa Dev Team - Security & Tools Updater

echo ========================================================
echo  湯川開発チーム 脆弱性対策・ツール一括更新バッチ
echo ========================================================
echo.

:: ----------------------------------------------------------
:: 1. UAC 管理者権限チェック＆自動昇格 (アーキテクト設計)
:: ----------------------------------------------------------
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo [INFO] 管理者権限で再起動しています (UACプロンプトをご確認ください)...
    powershell -NoProfile -ExecutionPolicy Bypass -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b
)

echo [OK] 管理者権限で実行中
echo.

set "TEMP_DIR=C:\temp\yukawa_update"
if not exist "%TEMP_DIR%" mkdir "%TEMP_DIR%"

:: ----------------------------------------------------------
:: 2. Node.js 更新 (CVE-2026-111497 修正版 v26.12.1)
:: ----------------------------------------------------------
echo --------------------------------------------------------
echo [1/3] Node.js を最新パッチ版 (v26.12.1) に更新中...
echo --------------------------------------------------------
set "NODE_MSI=%TEMP_DIR%\node-v26.12.1-x64.msi"

echo >> インストーラーをダウンロードしています...
curl.exe -L -o "%NODE_MSI%" "https://nodejs.org/dist/v26.12.1/node-v26.12.1-x64.msi"
if %errorlevel% neq 0 (
    echo [ERROR] Node.js のダウンロードに失敗しました。
) else (
    echo >> サイレントインストールを実行しています...
    msiexec.exe /i "%NODE_MSI%" /qn /norestart
    if %errorlevel% equ 0 (
        echo [OK] Node.js v26.12.1 の適用に成功しました。
    ) else (
        echo [WARN] Node.js のインストーラーがコード %errorlevel% で終了しました。
    )
)
echo.

:: ----------------------------------------------------------
:: 3. Java (JDK) 更新 (CVE-2026-91403 対策 Temurin 21 LTS)
:: ----------------------------------------------------------
echo --------------------------------------------------------
echo [2/3] Java (Temurin 21 LTS) を確認・更新中...
echo --------------------------------------------------------
winget install --id EclipseAdoptium.Temurin.21.JDK --accept-package-agreements --accept-source-agreements --silent
if %errorlevel% equ 0 (
    echo [OK] Eclipse Temurin 21 JDK は最新です。
) else (
    echo [INFO] winget 終了コード: %errorlevel%
)

:: システム環境変数 JAVA_HOME の同期
set "TEMURIN_DIR=C:\Program Files\Eclipse Adoptium\jdk-21.0.12.101-hotspot"
if exist "%TEMURIN_DIR%" (
    echo >> システム環境変数 JAVA_HOME を設定中: %TEMURIN_DIR%
    setx /M JAVA_HOME "%TEMURIN_DIR%" >nul
)
echo.

:: ----------------------------------------------------------
:: 4. GitLab (CE) コンテナ更新 (CVE-2026-110264 対策 v19.5.1)
:: ----------------------------------------------------------
echo --------------------------------------------------------
echo [3/3] Docker GitLab (CE) を最新版に更新中...
echo --------------------------------------------------------
docker info >nul 2>&1
if %errorlevel% neq 0 (
    echo [WARN] Docker デーモンが検出されませんでした。Docker Desktop を起動後に再実行してください。
) else (
    set "GITLAB_DIR=C:\Users\予備PC\.gemini\antigravity\scratch\gitlab"
    if exist "!GITLAB_DIR!\docker-compose.yml" (
        pushd "!GITLAB_DIR!"
        echo >> 最新の gitlab/gitlab-ce:latest イメージを取得中...
        docker compose pull
        echo >> コンテナを再作成・起動中...
        docker compose up -d
        popd
        echo [OK] GitLab CE コンテナの更新が完了しました。
    ) else (
        echo [WARN] GitLab compose ファイルが見つかりませんでした: !GITLAB_DIR!
    )
)
echo.

:: ----------------------------------------------------------
:: 5. クリーンアップ & 動作検証 (QA デバッグセッション)
:: ----------------------------------------------------------
echo --------------------------------------------------------
echo  動作確認 & バージョン検証
echo --------------------------------------------------------
if exist "%TEMP_DIR%" rmdir /s /q "%TEMP_DIR%" >nul 2>&1

echo [現在の Node.js バージョン]
"C:\Program Files\nodejs\node.exe" -v 2>nul || node -v 2>nul

echo.
echo [現在の Java バージョン]
"C:\Program Files\Eclipse Adoptium\jdk-21.0.12.101-hotspot\bin\java.exe" -version 2>&1 || java -version 2>&1

echo.
echo [GitLab コンテナ ステータス]
docker ps --filter "name=gitlab" --format "table {{.Names}}\t{{.Status}}\t{{.Ports}}" 2>nul

echo.
echo ========================================================
echo  すべてのアップデート処理が完了しました。
echo ========================================================
pause
