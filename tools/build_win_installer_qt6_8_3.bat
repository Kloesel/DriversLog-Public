@echo off
::chcp 65001 >nul
title Fahrtenbuch – Setup erstellen

:: ============================================================
::  PFADE ANPASSEN falls nötig
:: ============================================================
set PROJEKT=D:\QtSource\Fahrtenbuch
set BUILD_DIR=D:\QtSource\Fahrtenbuch\build\Desktop_Qt_6_8_3_MinGW_64_bit-Release
set DEPLOY=D:\QtSource\Fahrtenbuch_Deploy
set INSTALLER=D:\QtSource\Fahrtenbuch_Installer
set QT_BIN=C:\Qt\6.8.3\mingw_64\bin
set MINGW_BIN=C:\Qt\Tools\mingw1310_64\bin
set IFW_BIN=C:\Qt\Tools\QtInstallerFramework\4.10\bin
set DATA_DIR=%INSTALLER%\packages\com.fahrtenbuch.app\data
:: ============================================================

:: Version aus Fahrtenbuch.pro lesen (nur Release-Zeile, ohne BUILD_TIME)
for /f "tokens=3" %%A in ('findstr /c:"    VERSION = 1." "%PROJEKT%\Fahrtenbuch.pro" ^| findstr /v "BUILD_TIME"') do set VERSION=%%A
:: Nur Major.Minor.Patch sicherstellen
for /f "tokens=1-3 delims=." %%A in ("%VERSION%") do set VERSION=%%A.%%B.%%C
echo [INFO] Version: %VERSION%
echo.

set PATH=%MINGW_BIN%;%QT_BIN%;%PATH%

echo.
echo ============================================================
echo  Fahrtenbuch Setup-Ersteller
echo ============================================================
echo.

:: Schritt 1 – Build-Verzeichnis vorbereiten und qmake ausführen
echo [1/6] Erzeuge Makefile mit qmake (Release)...
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd /d "%BUILD_DIR%"
"%QT_BIN%\qmake.exe" "%PROJEKT%\Fahrtenbuch.pro" -spec win32-g++ "CONFIG+=release" "CONFIG-=debug"
if %ERRORLEVEL% neq 0 (
    echo FEHLER beim Ausfuehren von qmake!
    pause
    exit /b 1
)
echo     OK
echo.

:: Schritt 2 – Kompilieren
echo [2/6] Kompiliere Fahrtenbuch (Release)...
mingw32-make -j4
if %ERRORLEVEL% neq 0 (
    echo FEHLER beim Kompilieren!
    pause
    exit /b 1
)
echo     OK
echo.

:: Schritt 3 – EXE in Deploy-Ordner kopieren
echo [3/6] Kopiere Fahrtenbuch.exe in Deploy-Ordner...
if not exist "%DEPLOY%" mkdir "%DEPLOY%"
copy /Y "%BUILD_DIR%\release\Fahrtenbuch.exe" "%DEPLOY%\" >nul
if %ERRORLEVEL% neq 0 (
    echo FEHLER: Fahrtenbuch.exe nicht gefunden in %BUILD_DIR%\release\
    echo Bitte BUILD_DIR im Skript pruefen.
    pause
    exit /b 1
)
echo     OK
echo.

:: Schritt 4 – windeployqt
echo [4/6] Aktualisiere Qt-DLLs mit windeployqt...
cd /d "%DEPLOY%"
"%QT_BIN%\windeployqt.exe" --no-system-d3d-compiler Fahrtenbuch.exe >nul
echo     OK
echo.

:: Schritt 5 – Dateien ins data-Verzeichnis kopieren
echo [5/6] Kopiere Dateien ins Installer-Datenverzeichnis...
if not exist "%DATA_DIR%" mkdir "%DATA_DIR%"
xcopy /Y /E /I "%DEPLOY%\*" "%DATA_DIR%\" >nul
if %ERRORLEVEL% neq 0 (
    echo FEHLER beim Kopieren ins data-Verzeichnis!
    pause
    exit /b 1
)
echo     OK
echo.

:: Schritt 6 – Installer bauen
echo [6/6] Erstelle DriversLog_%VERSION%_Setup.exe...
cd /d "%INSTALLER%"
if exist DriversLog_*_Setup.exe del DriversLog_*_Setup.exe
"%IFW_BIN%\binarycreator.exe" --offline-only -c config/config.xml -p packages DriversLog_%VERSION%_Setup.exe
if %ERRORLEVEL% neq 0 (
    echo FEHLER beim Erstellen des Installers!
    pause
    exit /b 1
)
echo     OK
echo.

echo ============================================================
echo  FERTIG! Installer wurde erstellt:
echo  %INSTALLER%\DriversLog_%VERSION%_Setup.exe
echo ============================================================
echo.
pause
