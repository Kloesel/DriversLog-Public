#pragma once
#include <QString>
#include <QFileInfo>

// Settings: DB-Pfad wird unter Windows in der Registry gespeichert:
//   HKEY_CURRENT_USER\SOFTWARE\Kloesel\Fahrtenbuch\Database\DatabasePath
// Unter Android: SharedPreferences (Qt NativeFormat)
// Beim ersten Start nach Migration wird ein vorhandener INI-Wert übernommen.

class Settings
{
public:
    static QString getDatabasePath();
    static void    setDatabasePath(const QString &path);
    static QString getDefaultDatabasePath();

    // Einmalige Migration INI → Registry (wird von main.cpp aufgerufen)
    static void migrateFromIniIfNeeded();
};
