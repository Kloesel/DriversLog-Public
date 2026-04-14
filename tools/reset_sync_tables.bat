@echo off
chcp 65001 >nul
title Sync-Tabellen zuruecksetzen

set DB=%APPDATA%\Kloesel\Fahrtenbuch\fahrtenbuch.db
set SQLITE=C:\Users\klaus\AppData\Local\Android\Sdk\platform-tools\sqlite3.exe

echo.
echo ============================================================
echo  Fahrtenbuch - Sync-Tabellen zuruecksetzen
echo ============================================================
echo.
echo  Datenbank: %DB%
echo.

if not exist "%DB%" (
    echo [FEHLER] Datenbank nicht gefunden: %DB%
    pause
    exit /b 1
)

if not exist "%SQLITE%" (
    echo [FEHLER] sqlite3.exe nicht gefunden: %SQLITE%
    pause
    exit /b 1
)

echo [WARNUNG] Alle Sync-Daten werden geloescht!
echo           Nutzerdaten bleiben erhalten.
echo.
set /p CONFIRM=Fortfahren? (j/n): 
if /i not "%CONFIRM%"=="j" (
    echo Abgebrochen.
    pause
    exit /b 0
)

echo.
echo Leere Sync-Tabellen...
"%SQLITE%" "%DB%" "DELETE FROM sync_log; DELETE FROM sync_knowledge; DELETE FROM sync_meta; DELETE FROM sqlite_sequence WHERE name IN ('sync_log','sync_knowledge','sync_meta'); VACUUM;"
if errorlevel 1 (
    echo [FEHLER] SQL fehlgeschlagen.
    pause
    exit /b 1
)

echo.
echo ============================================================
echo  FERTIG. Bitte Fahrtenbuch neu starten.
echo ============================================================
echo.
pause
