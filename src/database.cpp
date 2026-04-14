#include "database.h"
#include "settings.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>
#include <QFile>
#include <QSettings>

Database::Database(QObject *parent) : QObject(parent) {}

Database::~Database() { close(); }

QString Database::defaultDbPath()
{
#if defined(Q_OS_ANDROID)
    return Settings::getDefaultDatabasePath();
#else
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + "/fahrtenbuch.db";
#endif
}

bool Database::open(const QString &path)
{
    QString dbPath;

    // WICHTIG: Reihenfolge beachten!
    // 1. INI prüfen/erstellen
    // 2. DB-Pfad aus INI lesen
    // 3. DB erstellen falls nicht existiert (am INI-Pfad!)
    // 4. DB öffnen (am INI-Pfad!)

    if (path == defaultDbPath()) {
        // Standard-Pfad übergeben → aus INI laden
        dbPath = Settings::getDatabasePath();
        qDebug() << "DB-Pfad aus INI geladen:" << dbPath;
    } else {
        // Custom Pfad direkt übergeben (z.B. für Tests)
        dbPath = path;
        qDebug() << "DB-Pfad direkt übergeben:" << dbPath;
    }

    // Sicherstellen dass DB-Verzeichnis existiert
    QFileInfo fi(dbPath);
    QDir().mkpath(fi.absolutePath());

    // DB öffnen (wird automatisch erstellt falls nicht vorhanden)
    m_db = QSqlDatabase::addDatabase("QSQLITE", "fahrtenbuch_conn");
    m_db.setDatabaseName(dbPath);

    if (!m_db.open()) {
        qDebug() << "DB konnte nicht geöffnet werden:" << dbPath;
        qDebug() << "SQLite-Fehler:" << m_db.lastError().text();
        qDebug() << "SQLite-Fehlertyp:" << m_db.lastError().type();
        return false;
    }

    qDebug() << "DB erfolgreich geöffnet:" << dbPath;

    QSqlQuery q(m_db);
    q.exec("PRAGMA foreign_keys = ON");
    q.exec("PRAGMA journal_mode = WAL");

    // Tabellen erstellen falls DB neu
    if (!createTables()) return false;

    // Schema-Migration: fehlende Spalten in bestehenden DBs ergänzen
    return migrate();
}

void Database::close()
{
    if (m_db.isOpen()) {
        {   // Extra-Scope: Query muss destroyed sein bevor m_db freigegeben wird
            QSqlQuery q(m_db);
            q.exec("PRAGMA wal_checkpoint(TRUNCATE)");  // WAL vollständig in DB-Datei schreiben
            q.exec("PRAGMA journal_mode = DELETE");     // WAL deaktivieren
        }
        m_db.close();
    }
    m_db = QSqlDatabase();  // Member-Referenz freigeben bevor removeDatabase
    QSqlDatabase::removeDatabase("fahrtenbuch_conn");
}

bool Database::isOpen() const { return m_db.isOpen(); }
QString Database::lastError() const { return m_db.lastError().text(); }

bool Database::createTables()
{
    QSqlQuery q(m_db);

    bool ok = q.exec(R"(
        CREATE TABLE IF NOT EXISTS fahrer (
            id         INTEGER PRIMARY KEY AUTOINCREMENT,
            name       TEXT NOT NULL,
            vorname    TEXT,
            kuerzel    TEXT,
            is_deleted  INTEGER DEFAULT 0,
            is_default  INTEGER DEFAULT 0
        )
    )");
    if (!ok) { qWarning() << q.lastError(); return false; }

    ok = q.exec(R"(
        CREATE TABLE IF NOT EXISTS adressen (
            id           INTEGER PRIMARY KEY AUTOINCREMENT,
            bezeichnung  TEXT,
            strasse      TEXT,
            hausnummer   TEXT,
            plz          TEXT,
            ort          TEXT NOT NULL,
            land         TEXT DEFAULT 'Deutschland',
            latitude     REAL DEFAULT 0,
            longitude    REAL DEFAULT 0,
            is_deleted   INTEGER DEFAULT 0
        )
    )");
    if (!ok) { qWarning() << q.lastError(); return false; }

    ok = q.exec(R"(
        CREATE TABLE IF NOT EXISTS fahrten (
            id               INTEGER PRIMARY KEY AUTOINCREMENT,
            datum            TEXT NOT NULL,
            fahrer_id        INTEGER REFERENCES fahrer(id),
            start_adresse_id INTEGER REFERENCES adressen(id),
            ziel_adresse_id  INTEGER REFERENCES adressen(id),
            entfernung       REAL DEFAULT 0,
            hin_und_zurueck  INTEGER DEFAULT 0,
            bemerkung        TEXT,
            is_deleted       INTEGER DEFAULT 0
        )
    )");
    if (!ok) { qWarning() << q.lastError(); return false; }

    ok = q.exec(R"(
        CREATE TABLE IF NOT EXISTS einstellungen (
            key   TEXT PRIMARY KEY,
            value TEXT
        )
    )");
    if (!ok) { qWarning() << q.lastError(); return false; }

    return true;
}

bool Database::migrate()
{
    // Idempotent: fehlende Spalten ergänzen, bestehende ignorieren.
    // ALTER TABLE ADD COLUMN schlägt mit Fehler fehl wenn Spalte bereits existiert,
    // daher prüfen wir erst per PRAGMA table_info.

    auto hasColumn = [&](const QString &table, const QString &col) -> bool {
        QSqlQuery q(m_db);
        q.exec(QString("PRAGMA table_info(%1)").arg(table));
        while (q.next())
            if (q.value("name").toString() == col) return true;
        return false;
    };

    auto addCol = [&](const QString &table, const QString &col,
                      const QString &type) -> bool {
        if (hasColumn(table, col)) return true;  // bereits vorhanden
        QSqlQuery q(m_db);
        bool ok = q.exec(QString("ALTER TABLE %1 ADD COLUMN %2 %3")
                         .arg(table, col, type));
        if (!ok) qWarning() << "[DB] migrate: ALTER TABLE" << table
                             << col << q.lastError().text();
        else     qDebug() << "[DB] migrate: Spalte hinzugefügt:" << table << col;
        return ok;
    };

    // v1.4.10 – Tombstone-Spalten für Soft Delete
    // DEFAULT 0 = nicht gelöscht; keine bestehenden Zeilen werden als gelöscht markiert.
    if (!addCol("fahrten",  "is_deleted", "INTEGER DEFAULT 0")) return false;
    if (!addCol("adressen", "is_deleted", "INTEGER DEFAULT 0")) return false;
    if (!addCol("fahrer",   "is_deleted", "INTEGER DEFAULT 0")) return false;
    if (!addCol("fahrer",   "is_default", "INTEGER DEFAULT 0")) return false;

    // v1.4.22 – Default-Fahrer anlegen falls noch nicht vorhanden
    {
        QSqlQuery chk(m_db);
        chk.prepare("SELECT COUNT(*) FROM fahrer WHERE is_default=1 AND is_deleted=0");
        if (chk.exec() && chk.next() && chk.value(0).toInt() == 0) {
            QSqlQuery ins(m_db);
            ins.prepare("INSERT INTO fahrer (name, is_default) VALUES ('unbekannt', 1)");
            ins.exec();
        }
    }

    return true;
}

// ────────────────────────────────────────────────────────────────────────────
// Fahrten
// ────────────────────────────────────────────────────────────────────────────
QVector<Fahrt> Database::getAllFahrten(int monat, int jahr,
                                       const QString &sortField,
                                       const QString &sortDir)
{
    QVector<Fahrt> result;

    // Whitelist erlaubter Sortierfelder – verhindert SQL-Injection
    static const QStringList allowedFields = {
        "datum", "fahrer_id", "start_adresse_id",
        "ziel_adresse_id", "entfernung", "bemerkung"
    };
    static const QStringList allowedDirs = { "ASC", "DESC" };

    const QString safeField = allowedFields.contains(sortField) ? sortField : "datum";
    const QString safeDir   = allowedDirs.contains(sortDir.toUpper()) ? sortDir.toUpper() : "DESC";

    QString sql = R"(
        SELECT f.id, f.datum, f.fahrer_id,
               fa.name AS fahrer_name,
               f.start_adresse_id,
               COALESCE(sa.bezeichnung, sa.strasse||' '||sa.hausnummer||', '||sa.plz||' '||sa.ort) AS start_bez,
               f.ziel_adresse_id,
               COALESCE(za.bezeichnung, za.strasse||' '||za.hausnummer||', '||za.plz||' '||za.ort) AS ziel_bez,
               f.entfernung, f.hin_und_zurueck, f.bemerkung
        FROM fahrten f
        LEFT JOIN fahrer fa ON fa.id = f.fahrer_id
        LEFT JOIN adressen sa ON sa.id = f.start_adresse_id
        LEFT JOIN adressen za ON za.id = f.ziel_adresse_id
        WHERE f.is_deleted = 0
    )";
    if (jahr  > 0) sql += " AND strftime('%Y', f.datum) = :jahr";
    if (monat > 0) sql += " AND strftime('%m', f.datum) = :monat";

    // Dynamische Sortierung – bei Adressfeldern nach dem Anzeigetext statt ID sortieren
    QString orderField;
    if      (safeField == "start_adresse_id") orderField = "start_bez";
    else if (safeField == "ziel_adresse_id")  orderField = "ziel_bez";
    else if (safeField == "fahrer_id")        orderField = "fahrer_name";
    else                                      orderField = "f." + safeField;

    sql += QString(" ORDER BY %1 %2, f.id %3").arg(orderField, safeDir, safeDir);

    QSqlQuery q(m_db);
    q.prepare(sql);
    if (jahr  > 0) q.bindValue(":jahr",  QString::number(jahr));
    if (monat > 0) q.bindValue(":monat", QString("%1").arg(monat, 2, 10, QChar('0')));

    if (!q.exec()) { qWarning() << q.lastError(); return result; }

    while (q.next()) {
        Fahrt fahrt;
        fahrt.id             = q.value(0).toInt();
        fahrt.datum          = QDate::fromString(q.value(1).toString(), Qt::ISODate);
        fahrt.fahrerId       = q.value(2).toInt();
        fahrt.fahrerName     = q.value(3).toString().trimmed();
        fahrt.startAdresseId = q.value(4).toInt();
        fahrt.startAdresse   = q.value(5).toString();
        fahrt.zielAdresseId  = q.value(6).toInt();
        fahrt.zielAdresse    = q.value(7).toString();
        fahrt.entfernung     = q.value(8).toDouble();
        fahrt.hinUndZurueck  = q.value(9).toBool();
        fahrt.bemerkung      = q.value(10).toString();
        result.append(fahrt);
    }
    return result;
}

bool Database::insertFahrt(Fahrt &fahrt)
{
    QSqlQuery q(m_db);
    q.prepare(R"(
        INSERT INTO fahrten (datum, fahrer_id, start_adresse_id, ziel_adresse_id,
                             entfernung, hin_und_zurueck, bemerkung)
        VALUES (:datum, :fid, :sid, :zid, :dist, :hz, :bem)
    )");
    q.bindValue(":datum", fahrt.datum.toString(Qt::ISODate));
    q.bindValue(":fid",   fahrt.fahrerId       > 0 ? QVariant(fahrt.fahrerId)       : QVariant(QMetaType::fromType<int>()));
    q.bindValue(":sid",   fahrt.startAdresseId > 0 ? QVariant(fahrt.startAdresseId) : QVariant(QMetaType::fromType<int>()));
    q.bindValue(":zid",   fahrt.zielAdresseId  > 0 ? QVariant(fahrt.zielAdresseId)  : QVariant(QMetaType::fromType<int>()));
    q.bindValue(":dist",  fahrt.entfernung);
    q.bindValue(":hz",    fahrt.hinUndZurueck ? 1 : 0);
    q.bindValue(":bem",   fahrt.bemerkung);
    if (!q.exec()) { qWarning() << q.lastError(); return false; }
    fahrt.id = q.lastInsertId().toInt();
    return true;
}

Fahrt Database::getFahrtById(int id)
{
    QSqlQuery q(m_db);
    q.prepare(R"(
        SELECT f.id, f.datum, f.fahrer_id,
               fa.name AS fahrer_name,
               f.start_adresse_id,
               COALESCE(sa.bezeichnung, sa.strasse||' '||sa.hausnummer) AS start_bez,
               f.ziel_adresse_id,
               COALESCE(za.bezeichnung, za.strasse||' '||za.hausnummer) AS ziel_bez,
               f.entfernung, f.hin_und_zurueck, f.bemerkung
        FROM fahrten f
        LEFT JOIN fahrer fa ON fa.id = f.fahrer_id
        LEFT JOIN adressen sa ON sa.id = f.start_adresse_id
        LEFT JOIN adressen za ON za.id = f.ziel_adresse_id
        WHERE f.id = :id AND f.is_deleted = 0
    )");
    q.bindValue(":id", id);
    Fahrt f;
    if (!q.exec() || !q.next()) return f;
    f.id             = q.value(0).toInt();
    f.datum          = QDate::fromString(q.value(1).toString(), Qt::ISODate);
    f.fahrerId       = q.value(2).toInt();
    f.fahrerName     = q.value(3).toString().trimmed();
    f.startAdresseId = q.value(4).toInt();
    f.startAdresse   = q.value(5).toString();
    f.zielAdresseId  = q.value(6).toInt();
    f.zielAdresse    = q.value(7).toString();
    f.entfernung     = q.value(8).toDouble();
    f.hinUndZurueck  = q.value(9).toBool();
    f.bemerkung      = q.value(10).toString();
    return f;
}

bool Database::updateFahrt(const Fahrt &fahrt){
    QSqlQuery q(m_db);
    q.prepare(R"(
        UPDATE fahrten SET datum=:datum, fahrer_id=:fid, start_adresse_id=:sid,
            ziel_adresse_id=:zid, entfernung=:dist, hin_und_zurueck=:hz, bemerkung=:bem
        WHERE id=:id
    )");
    q.bindValue(":datum", fahrt.datum.toString(Qt::ISODate));
    q.bindValue(":fid",   fahrt.fahrerId       > 0 ? QVariant(fahrt.fahrerId)       : QVariant(QMetaType::fromType<int>()));
    q.bindValue(":sid",   fahrt.startAdresseId > 0 ? QVariant(fahrt.startAdresseId) : QVariant(QMetaType::fromType<int>()));
    q.bindValue(":zid",   fahrt.zielAdresseId  > 0 ? QVariant(fahrt.zielAdresseId)  : QVariant(QMetaType::fromType<int>()));
    q.bindValue(":dist",  fahrt.entfernung);
    q.bindValue(":hz",    fahrt.hinUndZurueck ? 1 : 0);
    q.bindValue(":bem",   fahrt.bemerkung);
    q.bindValue(":id",    fahrt.id);
    if (!q.exec()) { qWarning() << q.lastError(); return false; }

    // Debug: Trigger-Verifikation – wurde ein sync_log-Eintrag angelegt?
    {
        QSqlQuery chk(m_db);
        chk.exec("SELECT COUNT(*) FROM sync_log WHERE table_name='fahrten' "
                 "AND status='pending' AND direction='local' AND timestamp_ms > 1000");
        if (chk.next())
            qDebug() << "[DB] updateFahrt: sync_log pending-Einträge für fahrten:"
                     << chk.value(0).toInt();
    }
    return true;
}

bool Database::deleteFahrt(int id)
{
    // Soft Delete: Tombstone setzen statt physisch löschen.
    // Andere Geräte erfahren so via Sync dass diese Fahrt entfernt wurde.
    QSqlQuery q(m_db);
    q.prepare("UPDATE fahrten SET is_deleted=1 WHERE id=:id");
    q.bindValue(":id", id);
    return q.exec();
}

QVector<Fahrt> Database::getFahrtenForExport(int monat, int jahr)
{
    // Export immer chronologisch aufsteigend
    return getAllFahrten(monat, jahr, "datum", "ASC");
}

// ────────────────────────────────────────────────────────────────────────────
// Adressen
// ────────────────────────────────────────────────────────────────────────────
QVector<Adresse> Database::getAllAdressen()
{
    QVector<Adresse> result;
    QSqlQuery q("SELECT id,bezeichnung,strasse,hausnummer,plz,ort,land,latitude,longitude FROM adressen WHERE is_deleted=0 ORDER BY bezeichnung,ort", m_db);
    while (q.next()) {
        Adresse a;
        a.id           = q.value(0).toInt();
        a.bezeichnung  = q.value(1).toString();
        a.strasse      = q.value(2).toString();
        a.hausnummer   = q.value(3).toString();
        a.plz          = q.value(4).toString();
        a.ort          = q.value(5).toString();
        a.land         = q.value(6).toString();
        a.latitude     = q.value(7).toDouble();
        a.longitude    = q.value(8).toDouble();
        result.append(a);
    }
    return result;
}

Adresse Database::getAdresseById(int id)
{
    Adresse a;
    QSqlQuery q(m_db);
    q.prepare("SELECT id,bezeichnung,strasse,hausnummer,plz,ort,land,latitude,longitude FROM adressen WHERE id=:id AND is_deleted=0");
    q.bindValue(":id", id);
    if (q.exec() && q.next()) {
        a.id          = q.value(0).toInt();
        a.bezeichnung = q.value(1).toString();
        a.strasse     = q.value(2).toString();
        a.hausnummer  = q.value(3).toString();
        a.plz         = q.value(4).toString();
        a.ort         = q.value(5).toString();
        a.land        = q.value(6).toString();
        a.latitude    = q.value(7).toDouble();
        a.longitude   = q.value(8).toDouble();
    }
    return a;
}

bool Database::insertAdresse(Adresse &adresse)
{
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO adressen (bezeichnung,strasse,hausnummer,plz,ort,land,latitude,longitude) VALUES (:b,:s,:h,:p,:o,:l,:lat,:lon)");
    q.bindValue(":b",   adresse.bezeichnung);
    q.bindValue(":s",   adresse.strasse);
    q.bindValue(":h",   adresse.hausnummer);
    q.bindValue(":p",   adresse.plz);
    q.bindValue(":o",   adresse.ort);
    q.bindValue(":l",   adresse.land);
    q.bindValue(":lat", adresse.latitude);
    q.bindValue(":lon", adresse.longitude);
    if (!q.exec()) { qWarning() << q.lastError(); return false; }
    adresse.id = q.lastInsertId().toInt();
    return true;
}

bool Database::updateAdresse(const Adresse &adresse)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE adressen SET bezeichnung=:b,strasse=:s,hausnummer=:h,plz=:p,ort=:o,land=:l,latitude=:lat,longitude=:lon WHERE id=:id");
    q.bindValue(":b",   adresse.bezeichnung);
    q.bindValue(":s",   adresse.strasse);
    q.bindValue(":h",   adresse.hausnummer);
    q.bindValue(":p",   adresse.plz);
    q.bindValue(":o",   adresse.ort);
    q.bindValue(":l",   adresse.land);
    q.bindValue(":lat", adresse.latitude);
    q.bindValue(":lon", adresse.longitude);
    q.bindValue(":id",  adresse.id);
    if (!q.exec()) { qWarning() << q.lastError(); return false; }
    return true;
}

bool Database::updateAdresseCoords(int id, double lat, double lon)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE adressen SET latitude=:lat, longitude=:lon WHERE id=:id");
    q.bindValue(":lat", lat);
    q.bindValue(":lon", lon);
    q.bindValue(":id",  id);
    if (!q.exec()) { qWarning() << "updateAdresseCoords:" << q.lastError(); return false; }
    qDebug() << "Koordinaten gespeichert fuer Adresse" << id << lat << lon;
    return true;
}

bool Database::adresseInFahrtenUsed(int id)
{
    QSqlQuery q(m_db);
    q.prepare("SELECT COUNT(*) FROM fahrten "
              "WHERE (start_adresse_id=:id OR ziel_adresse_id=:id) AND is_deleted=0");
    q.bindValue(":id", id);
    if (q.exec() && q.next())
        return q.value(0).toInt() > 0;
    return false;
}

int Database::findDuplicateAdresseId(const QString &strasse, const QString &hausnummer,
                                      const QString &plz,     const QString &ort,
                                      int excludeId)
{
    QSqlQuery q(m_db);
    q.prepare("SELECT id FROM adressen "
              "WHERE LOWER(strasse)=LOWER(:s) AND LOWER(hausnummer)=LOWER(:h) "
              "  AND LOWER(plz)=LOWER(:p)    AND LOWER(ort)=LOWER(:o) "
              "  AND id != :excl "
              "  AND (is_deleted IS NULL OR is_deleted = 0) "
              "LIMIT 1");
    q.bindValue(":s",    strasse.trimmed());
    q.bindValue(":h",    hausnummer.trimmed());
    q.bindValue(":p",    plz.trimmed());
    q.bindValue(":o",    ort.trimmed());
    q.bindValue(":excl", excludeId);
    if (q.exec() && q.next())
        return q.value(0).toInt();
    return 0;
}

bool Database::deleteAdresse(int id)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE adressen SET is_deleted=1 WHERE id=:id");
    q.bindValue(":id", id);
    return q.exec();
}

// ────────────────────────────────────────────────────────────────────────────
// Fahrer
// ────────────────────────────────────────────────────────────────────────────
QVector<Fahrer> Database::getAllFahrer()
{
    QVector<Fahrer> result;
    QSqlQuery q("SELECT id,name,is_default FROM fahrer WHERE is_deleted=0 ORDER BY name", m_db);
    while (q.next()) {
        Fahrer f;
        f.id        = q.value(0).toInt();
        f.name      = q.value(1).toString();
        f.isDefault = q.value(2).toBool();
        result.append(f);
    }
    return result;
}

Fahrer Database::getFahrerById(int id)
{
    Fahrer f;
    QSqlQuery q(m_db);
    q.prepare("SELECT id,name,is_default FROM fahrer WHERE id=:id AND is_deleted=0");
    q.bindValue(":id", id);
    if (q.exec() && q.next()) {
        f.id        = q.value(0).toInt();
        f.name      = q.value(1).toString();
        f.isDefault = q.value(2).toBool();
    }
    return f;
}

int Database::getDefaultFahrerId()
{
    QSqlQuery q(m_db);
    q.prepare("SELECT id FROM fahrer WHERE is_default=1 AND is_deleted=0 LIMIT 1");
    if (q.exec() && q.next())
        return q.value(0).toInt();
    return 0;
}

bool Database::insertFahrer(Fahrer &fahrer)
{
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO fahrer (name) VALUES (:n)");
    q.bindValue(":n", fahrer.name);
    if (!q.exec()) { qWarning() << q.lastError(); return false; }
    fahrer.id = q.lastInsertId().toInt();
    return true;
}

bool Database::updateFahrer(const Fahrer &fahrer)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE fahrer SET name=:n WHERE id=:id");
    q.bindValue(":n",  fahrer.name);
    q.bindValue(":id", fahrer.id);
    if (!q.exec()) { qWarning() << q.lastError(); return false; }
    return true;
}

bool Database::deleteFahrer(int id)
{
    // Default-Fahrer "unbekannt" kann nicht gelöscht werden
    if (id == getDefaultFahrerId()) return false;
    QSqlQuery q(m_db);
    q.prepare("UPDATE fahrer SET is_deleted=1 WHERE id=:id");
    q.bindValue(":id", id);
    return q.exec();
}

// ────────────────────────────────────────────────────────────────────────────
// Einstellungen
// ────────────────────────────────────────────────────────────────────────────
Einstellungen Database::getEinstellungen()
{
    Einstellungen e;
    QSqlQuery q("SELECT key,value FROM einstellungen", m_db);
    while (q.next()) {
        QString key = q.value(0).toString();
        QString val = q.value(1).toString();
        if      (key == "standardFahrerId")   e.standardFahrerId    = val.toInt();
        else if (key == "standardAdresseId")  e.standardAdresseId   = val.toInt();
        else if (key == "standardHinZurueck") e.standardHinZurueck  = val == "1";
        // filterMonat/filterJahr werden NICHT aus der DB gelesen –
        // sie stehen in QSettings (gerätespezifisch, nicht per Dropbox geteilt).
        else if (key == "syncMode" && !val.isEmpty()) e.syncMode = val;
        else if (key == "wifiUdpPort")        e.wifiUdpPort         = val.toInt();
        else if (key == "wifiTcpPort")        e.wifiTcpPort         = val.toInt();
        // Rückwärtskompatibilität: alter syncPort → wifiTcpPort
        else if (key == "syncPort" && e.wifiTcpPort == 45454) {
            int p = val.toInt();
            if (p > 1024) e.wifiTcpPort = p;
        }
        else if (key == "apiKeyDistance")     e.apiKeyDistance      = val;
        else if (key == "databasePath")       e.databasePath        = val;
        else if (key == "mehrerefahrer")      e.mehrerefahrer       = val != "0";
        else if (key == "language")            e.language            = val;

    }

    // Gerätespezifische Einstellungen aus QSettings
    {
        QSettings s;
        s.beginGroup("Filter");
        e.filterMonat = s.value("filterMonat", 0).toInt();
        e.filterJahr  = s.value("filterJahr",  0).toInt();
        s.endGroup();
        e.language = s.value(QStringLiteral("app/language"), QString()).toString();
    }
    return e;
}

bool Database::saveEinstellungen(const Einstellungen &e)
{
    auto set = [&](const QString &key, const QString &val) {
        QSqlQuery q(m_db);
        q.prepare("INSERT OR REPLACE INTO einstellungen(key,value) VALUES(:k,:v)");
        q.bindValue(":k", key); q.bindValue(":v", val);
        return q.exec();
    };
    set("standardFahrerId",   QString::number(e.standardFahrerId));
    set("standardAdresseId",  QString::number(e.standardAdresseId));
    set("standardHinZurueck", e.standardHinZurueck ? "1" : "0");
    // filterMonat/filterJahr NICHT in die DB – gerätespezifisch in QSettings
    set("syncMode",           e.syncMode);
    set("wifiUdpPort",        QString::number(e.wifiUdpPort));
    set("wifiTcpPort",        QString::number(e.wifiTcpPort));
    set("apiKeyDistance",     e.apiKeyDistance);
    set("databasePath",       e.databasePath);
    set("mehrerefahrer",      e.mehrerefahrer ? "1" : "0");

    // Gerätespezifisch in QSettings speichern (Sprache + Filter)
    {
        QSettings s;
        s.setValue(QStringLiteral("app/language"), e.language);
        s.beginGroup("Filter");
        s.setValue("filterMonat", e.filterMonat);
        s.setValue("filterJahr",  e.filterJahr);
        s.endGroup();
        s.sync();
    }
    return true;
}
