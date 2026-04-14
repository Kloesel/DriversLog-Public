#pragma once
#include <QString>
#include <QDate>

// ─── Fahrer ────────────────────────────────────────────────────────────────
struct Fahrer {
    int     id        = 0;
    QString name;
    bool    isDefault = false;  // true = Default-Fahrer, kann nicht gelöscht werden
};

// ─── Adresse ───────────────────────────────────────────────────────────────
struct Adresse {
    int     id          = 0;
    QString bezeichnung;
    QString strasse;
    QString hausnummer;
    QString plz;
    QString ort;
    QString land        = "Deutschland";
    double  latitude    = 0.0;
    double  longitude   = 0.0;

    QString vollAdresse() const {
        return strasse + " " + hausnummer + ", " + plz + " " + ort;
    }
    QString anzeige() const {
        return bezeichnung.isEmpty() ? vollAdresse() : bezeichnung;
    }
};

// ─── Fahrt ─────────────────────────────────────────────────────────────────
struct Fahrt {
    int     id             = 0;
    QDate   datum;
    int     fahrerId       = 0;
    QString fahrerName;
    int     startAdresseId = 0;
    QString startAdresse;
    int     zielAdresseId  = 0;
    QString zielAdresse;
    double  entfernung     = 0.0;   // km, schon verdoppelt wenn hin+zurück
    bool    hinUndZurueck  = false;
    QString bemerkung;
};

// ─── Einstellungen ─────────────────────────────────────────────────────────
struct Einstellungen {
    QString databasePath;           // Benutzerdefinierter Datenbankpfad (leer = Standard)
    int     standardFahrerId     = 0;
    int     standardAdresseId    = 0;
    bool    standardHinZurueck   = false;
    int     filterMonat          = 0;   // 0 = kein Filter
    int     filterJahr           = 0;
    QString apiKeyDistance;             // z.B. ORS API Key

    // WLAN-Synchronisation
    QString syncMode             = QStringLiteral("wifi"); // \"wifi\" | \"off\" – Default: wifi aktiv
    int     wifiUdpPort          = 45455;   // UDP-Broadcast Port
    int     wifiTcpPort          = 45454;   // TCP-Transfer Port

    bool    mehrerefahrer        = true;    // false = kein Fahrer-Tab
    QString language;                       // "" = Systemsprache, "de", "en", ...
};
