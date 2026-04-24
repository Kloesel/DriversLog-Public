#include "databasesync.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QJsonDocument>
#include <algorithm>
#include <QVariant>
#include <QDebug>

// Reihenfolge wichtig: Abhängigkeiten zuerst (fahrer/adressen vor fahrten)
const QStringList DatabaseSync::kSyncTables = { "fahrer", "adressen", "fahrten" };

DatabaseSync::DatabaseSync(QSqlDatabase db,
                           const QString &deviceId,
                           QObject *parent)
    : QObject(parent)
    , m_db(db)
    , m_deviceId(deviceId)
{}

// ── Initialisierung ───────────────────────────────────────────────────────

bool DatabaseSync::initializeSync()
{
    qDebug() << "[DatabaseSync] initializeSync() start";
    if (!m_db.isOpen()) {
        qWarning() << "[DatabaseSync] DB nicht geöffnet";
        return false;
    }

    // Crash-Recovery: applying_remote-Flag beim Start immer zurücksetzen
    {
        QSqlQuery resetFlag(m_db);
        resetFlag.exec("INSERT OR REPLACE INTO sync_meta(key,value) "
                       "VALUES('applying_remote','0')");
        qDebug() << "[DatabaseSync] applying_remote-Flag zurückgesetzt";
    }

    qDebug() << "[DatabaseSync] rufe createSyncTables()";
    if (!createSyncTables()) {
        qWarning() << "[DatabaseSync] createSyncTables FEHLER";
        return false;
    }

    // Schema-Migration V2: local_seq + sync_knowledge
    if (!migrateSchemaV2()) {
        qWarning() << "[DatabaseSync] migrateSchemaV2 FEHLER";
        return false;
    }

    qDebug() << "[DatabaseSync] installiere Trigger...";
    for (const QString &table : kSyncTables) {
        if (!installTriggersForTable(table)) {
            qWarning() << "[DatabaseSync] Trigger-Fehler für:" << table;
            return false;
        }
    }

    migrateExistingData();

    qDebug() << "[DatabaseSync] initializeSync() fertig, device_id:" << m_deviceId;
    return true;
}

bool DatabaseSync::createSyncTables()
{
    QSqlQuery q(m_db);

    // sync_log: alle Änderungen mit Richtungsinfo
    // local_seq: pro-Gerät Sequenznummer (0 = alt/unbekannt, wird per migrateSchemaV2 gefüllt)
    if (!q.exec(
            "CREATE TABLE IF NOT EXISTS sync_log ("
            "  id           INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  table_name   TEXT    NOT NULL,"
            "  row_id       INTEGER NOT NULL,"
            "  operation    TEXT    NOT NULL,"    // INSERT | UPDATE | DELETE
            "  timestamp_ms INTEGER NOT NULL,"    // Unix-Millisekunden
            "  device_id    TEXT    NOT NULL,"
            "  direction    TEXT    NOT NULL DEFAULT 'local'," // local | remote
            "  status       TEXT    NOT NULL DEFAULT 'pending'," // pending|sent|received
            "  payload      TEXT,"               // JSON-Snapshot (für Relay)
            "  local_seq    INTEGER DEFAULT 0"   // Sequenznummer des Ursprungsgeräts
            ")")) {
        qWarning() << "[DatabaseSync] sync_log CREATE:" << q.lastError().text();
        return false;
    }

    q.exec("CREATE INDEX IF NOT EXISTS idx_sync_log_ts "
           "ON sync_log(timestamp_ms DESC)");
    q.exec("CREATE INDEX IF NOT EXISTS idx_sync_log_status "
           "ON sync_log(direction, status)");
    q.exec("CREATE INDEX IF NOT EXISTS idx_sync_log_seq "
           "ON sync_log(device_id, local_seq)");

    // sync_meta: Gerätekonfiguration & Flags
    if (!q.exec(
            "CREATE TABLE IF NOT EXISTS sync_meta ("
            "  key   TEXT PRIMARY KEY,"
            "  value TEXT"
            ")")) {
        qWarning() << "[DatabaseSync] sync_meta CREATE:" << q.lastError().text();
        return false;
    }

    // sync_knowledge: Knowledge Matrix – was hat welcher Peer von welchem Gerät
    // peer_device_id  : Gerät das die Änderungen empfangen hat
    // origin_device_id: Gerät das die Änderungen ursprünglich erzeugt hat
    // max_seq         : höchste local_seq die peer von origin bereits hat
    if (!q.exec(
            "CREATE TABLE IF NOT EXISTS sync_knowledge ("
            "  peer_device_id   TEXT NOT NULL,"
            "  origin_device_id TEXT NOT NULL,"
            "  max_seq          INTEGER NOT NULL DEFAULT 0,"
            "  PRIMARY KEY (peer_device_id, origin_device_id)"
            ")")) {
        qWarning() << "[DatabaseSync] sync_knowledge CREATE:" << q.lastError().text();
        return false;
    }

    q.exec("CREATE INDEX IF NOT EXISTS idx_knowledge_peer "
           "ON sync_knowledge(peer_device_id)");

    // device_id persistieren (für Trigger-Subselect)
    q.prepare("INSERT OR REPLACE INTO sync_meta(key,value) VALUES('device_id',?)");
    q.addBindValue(m_deviceId);
    q.exec();

    // applying_remote initialisieren
    q.exec("INSERT OR IGNORE INTO sync_meta(key,value) VALUES('applying_remote','0')");

    return true;
}

// ── Schema-Migration V2: local_seq-Spalte + seq-Zuweisung ────────────────

bool DatabaseSync::migrateSchemaV2()
{
    // Prüfen ob local_seq-Spalte bereits existiert
    bool hasLocalSeq = false;
    {
        QSqlQuery pragma(m_db);
        pragma.exec("PRAGMA table_info(sync_log)");
        while (pragma.next()) {
            if (pragma.value("name").toString() == QLatin1String("local_seq")) {
                hasLocalSeq = true;
                break;
            }
        }
    }

    if (!hasLocalSeq) {
        qDebug() << "[DatabaseSync] V2-Migration: füge local_seq hinzu...";
        QSqlQuery q(m_db);
        if (!q.exec("ALTER TABLE sync_log ADD COLUMN local_seq INTEGER DEFAULT 0")) {
            qWarning() << "[DatabaseSync] ALTER TABLE local_seq:" << q.lastError().text();
            return false;
        }
        qDebug() << "[DatabaseSync] local_seq-Spalte angelegt";
    }

    // Bestehenden Einträgen ohne local_seq (=0 oder NULL) Sequenznummern zuweisen.
    // Sortiert nach id (Einfügereihenfolge) pro device_id – ergibt konsistente Seqs.
    {
        QSqlQuery check(m_db);
        check.exec("SELECT COUNT(*) FROM sync_log WHERE local_seq = 0 OR local_seq IS NULL");
        int unassigned = check.next() ? check.value(0).toInt() : 0;

        if (unassigned > 0) {
            qDebug() << "[DatabaseSync] V2-Migration: weise" << unassigned
                     << "Einträgen Sequenznummern zu...";
            // Für jedes Gerät: Einträge aufsteigend nach id nummerieren
            // SQLite: ROW_NUMBER() erst ab 3.25 – Workaround mit korrelierten Subqueries
            QSqlQuery devQ(m_db);
            devQ.exec("SELECT DISTINCT device_id FROM sync_log "
                      "WHERE local_seq = 0 OR local_seq IS NULL");
            QStringList devices;
            while (devQ.next())
                devices << devQ.value(0).toString();

            m_db.transaction();
            for (const QString &dev : devices) {
                // Alle IDs für dieses Gerät aufsteigend laden
                QSqlQuery idQ(m_db);
                idQ.prepare("SELECT id FROM sync_log WHERE device_id=? "
                            "AND (local_seq=0 OR local_seq IS NULL) ORDER BY id ASC");
                idQ.addBindValue(dev);
                idQ.exec();

                // Höchste bereits vergebene Seq für dieses Gerät
                QSqlQuery maxQ(m_db);
                maxQ.prepare("SELECT COALESCE(MAX(local_seq),0) FROM sync_log "
                             "WHERE device_id=? AND local_seq > 0");
                maxQ.addBindValue(dev);
                maxQ.exec();
                int seq = maxQ.next() ? maxQ.value(0).toInt() : 0;

                QSqlQuery updQ(m_db);
                updQ.prepare("UPDATE sync_log SET local_seq=? WHERE id=?");
                while (idQ.next()) {
                    ++seq;
                    updQ.addBindValue(seq);
                    updQ.addBindValue(idQ.value(0).toInt());
                    updQ.exec();
                }
                qDebug() << "[DatabaseSync] V2-Migration: Gerät" << dev.left(8)
                         << "– Seqs bis" << seq << "vergeben";
            }
            m_db.commit();
        }
    }

    return true;
}

bool DatabaseSync::installTriggersForTable(const QString &tableName)
{
    QStringList cols = columnNames(tableName);
    if (cols.isEmpty()) {
        qWarning() << "[DatabaseSync] installTriggers: keine Spalten für" << tableName;
        return false;
    }

    const QString ts    = "(CAST(strftime('%s','now') AS INTEGER) * 1000)";
    const QString dev   = "(COALESCE((SELECT value FROM sync_meta WHERE key='device_id'),'unknown'))";
    const QString guard = "COALESCE((SELECT value FROM sync_meta WHERE key='applying_remote'),'0') = '0'";
    // Sequenznummer: monoton steigend pro device_id innerhalb sync_log
    const QString seq   = "(SELECT COALESCE(MAX(local_seq),0)+1 FROM sync_log "
                          " WHERE device_id=(SELECT value FROM sync_meta WHERE key='device_id'))";

    const QString sqlInsert = QString(
        "CREATE TRIGGER sync_%1_insert "
        "AFTER INSERT ON %1 "
        "WHEN %2 BEGIN "
        "  INSERT INTO sync_log"
        "    (table_name,row_id,operation,timestamp_ms,device_id,direction,status,payload,local_seq) "
        "  VALUES('%1',NEW.id,'INSERT',%3,%4,'local','pending',NULL,%5); "
        "END;"
    ).arg(tableName, guard, ts, dev, seq);

    const QString sqlUpdate = QString(
        "CREATE TRIGGER sync_%1_update "
        "AFTER UPDATE ON %1 "
        "WHEN %2 BEGIN "
        "  INSERT INTO sync_log"
        "    (table_name,row_id,operation,timestamp_ms,device_id,direction,status,payload,local_seq) "
        "  VALUES('%1',NEW.id,'UPDATE',%3,%4,'local','pending',NULL,%5); "
        "END;"
    ).arg(tableName, guard, ts, dev, seq);

    const QString sqlDelete = QString(
        "CREATE TRIGGER sync_%1_delete "
        "BEFORE DELETE ON %1 "
        "WHEN %2 BEGIN "
        "  INSERT INTO sync_log"
        "    (table_name,row_id,operation,timestamp_ms,device_id,direction,status,payload,local_seq) "
        "  VALUES('%1',OLD.id,'DELETE',%3,%4,'local','pending','{}', %5); "
        "END;"
    ).arg(tableName, guard, ts, dev, seq);

    QSqlQuery q(m_db);

    // DROP existing triggers (best-effort — may fail if DB is locked or in WAL)
    for (const QString &op : { QStringLiteral("insert"), QStringLiteral("update"), QStringLiteral("delete") }) {
        QSqlQuery drop(m_db);
        if (!drop.exec(QString("DROP TRIGGER IF EXISTS sync_%1_%2").arg(tableName, op))) {
            qWarning() << "[DatabaseSync] DROP TRIGGER fehlgeschlagen:"
                       << drop.lastError().text();
        }
    }

    for (const QString &sql : { sqlInsert, sqlUpdate, sqlDelete }) {
        if (!q.exec(sql)) {
            const QString err = q.lastError().text();
            // "already exists" → Trigger ist noch da (DROP schlug fehl) → ignorieren
            if (err.contains(QLatin1String("already exists"), Qt::CaseInsensitive)) {
                qDebug() << "[DatabaseSync] Trigger existiert bereits (DROP fehlgeschlagen),"
                         << "überspringe:" << tableName;
                continue;
            }
            qWarning() << "[DatabaseSync] Trigger-Fehler:" << err
                       << "\nSQL:" << sql.left(120);
            return false;
        }
    }

    // Verifikation
    QSqlQuery vfy(m_db);
    vfy.prepare("SELECT COUNT(*) FROM sqlite_master WHERE type='trigger' AND name LIKE ?");
    vfy.addBindValue(QString("sync_%1_%").arg(tableName));
    if (vfy.exec() && vfy.next()) {
        int count = vfy.value(0).toInt();
        if (count != 3) {
            qWarning() << "[DatabaseSync] Trigger-Verifikation FEHLGESCHLAGEN für"
                       << tableName << "– gefunden:" << count << "von 3";
            return false;
        }
    }
    qDebug() << "[DatabaseSync] Trigger OK (verifiziert):" << tableName;
    return true;
}

// ── Einmalige Datenmigration ──────────────────────────────────────────────

bool DatabaseSync::migrateExistingData()
{
    QSqlQuery check(m_db);
    check.exec("SELECT value FROM sync_meta WHERE key='migrated'");
    if (check.next() && check.value(0).toString() == "1") {
        // Prüfen ob Migration mit korrektem Sentinel (ts=1) vorhanden
        QSqlQuery migTsCheck(m_db);
        migTsCheck.exec(
            "SELECT COUNT(*) FROM sync_log WHERE timestamp_ms = 1 AND direction='local'");
        if (migTsCheck.next() && migTsCheck.value(0).toInt() > 0) {
            qDebug() << "[DatabaseSync] Migration bereits korrekt durchgeführt (ts=1)";
            return true;
        }
        // Kein ts=1-Eintrag – entweder leere DB (ok) oder alte Migration (neu ausführen)
        // Leere DB prüfen: wenn alle Tabellen leer, war migration korrekt (nichts zu migrieren)
        bool allEmpty = true;
        for (const QString &table : kSyncTables) {
            QSqlQuery cntQ(m_db);
            cntQ.exec(QString("SELECT COUNT(*) FROM sync_log WHERE table_name='%1' "
                              "AND direction='local'").arg(table));
            if (cntQ.next() && cntQ.value(0).toInt() > 0) { allEmpty = false; break; }
        }
        if (allEmpty) {
            qDebug() << "[DatabaseSync] Migration OK – leere DB, kein Sentinel nötig";
            return true;
        }
        qDebug() << "[DatabaseSync] Alte Migration gefunden (ts≠1), führe neu durch...";
        QSqlQuery del(m_db);
        del.exec("DELETE FROM sync_log WHERE direction='local' AND status='pending'");
        QSqlQuery resetFlag(m_db);
        resetFlag.exec("DELETE FROM sync_meta WHERE key='migrated'");
    }

    qDebug() << "[DatabaseSync] Starte Erstmigration bestehender Daten...";

    // Migrations-Timestamp = 1 ms: jede echte Änderung (Trigger-Timestamp = aktuelle Zeit)
    // ist immer neuer → Konflikte zugunsten echter Änderungen aufgelöst.
    const qint64 migrationTs = 1LL;

    m_db.transaction();

    // Sequenz-Startpunkt für Migration: höchste vorhandene Seq + 1
    QSqlQuery maxSeqQ(m_db);
    maxSeqQ.prepare("SELECT COALESCE(MAX(local_seq),0) FROM sync_log WHERE device_id=?");
    maxSeqQ.addBindValue(m_deviceId);
    maxSeqQ.exec();
    int seq = maxSeqQ.next() ? maxSeqQ.value(0).toInt() : 0;

    for (const QString &table : kSyncTables) {
        QSqlQuery rows(m_db);
        rows.exec(QString("SELECT * FROM %1").arg(table));

        const QSqlRecord rec = rows.record();
        const int colCount   = rec.count();

        while (rows.next()) {
            QJsonObject obj;
            for (int i = 0; i < colCount; ++i)
                obj[rec.fieldName(i)] = QJsonValue::fromVariant(rows.value(i));

            const int rowId = rows.value("id").toInt();
            const QByteArray payload =
                QJsonDocument(obj).toJson(QJsonDocument::Compact);

            ++seq;

            QSqlQuery ins(m_db);
            ins.prepare(
                "INSERT OR IGNORE INTO sync_log"
                "  (table_name,row_id,operation,timestamp_ms,device_id,"
                "   direction,status,payload,local_seq) "
                "VALUES(?,?,?,?,?,'local','pending',?,?)");
            ins.addBindValue(table);
            ins.addBindValue(rowId);
            ins.addBindValue("INSERT");
            ins.addBindValue(migrationTs);
            ins.addBindValue(m_deviceId);
            ins.addBindValue(QString::fromUtf8(payload));
            ins.addBindValue(seq);
            ins.exec();
        }
        qDebug() << "[DatabaseSync] Migriert:" << table;
    }

    QSqlQuery flag(m_db);
    flag.exec("INSERT OR REPLACE INTO sync_meta(key,value) VALUES('migrated','1')");

    m_db.commit();
    qDebug() << "[DatabaseSync] Erstmigration abgeschlossen, Seqs bis" << seq;
    return true;
}

// ── Knowledge Matrix Helpers ──────────────────────────────────────────────

void DatabaseSync::updateKnowledge(const QString &peerDeviceId,
                                   const QString &originDeviceId,
                                   int maxSeq)
{
    if (peerDeviceId.isEmpty() || originDeviceId.isEmpty()) return;

    QSqlQuery q(m_db);
    // INSERT OR REPLACE + MAX stellt sicher dass wir nie rückwärts gehen
    q.prepare(
        "INSERT INTO sync_knowledge (peer_device_id, origin_device_id, max_seq) "
        "VALUES(?,?,?) "
        "ON CONFLICT(peer_device_id, origin_device_id) "
        "DO UPDATE SET max_seq = MAX(max_seq, excluded.max_seq)");
    q.addBindValue(peerDeviceId);
    q.addBindValue(originDeviceId);
    q.addBindValue(maxSeq);
    if (!q.exec())
        qWarning() << "[DatabaseSync] updateKnowledge Fehler:" << q.lastError().text();
}

int DatabaseSync::getKnownSeq(const QString &peerDeviceId,
                               const QString &originDeviceId) const
{
    QSqlQuery q(m_db);
    q.prepare("SELECT max_seq FROM sync_knowledge "
              "WHERE peer_device_id=? AND origin_device_id=?");
    q.addBindValue(peerDeviceId);
    q.addBindValue(originDeviceId);
    if (q.exec() && q.next())
        return q.value(0).toInt();
    return 0;
}

// ── Export ────────────────────────────────────────────────────────────────

QJsonArray DatabaseSync::exportChanges(const QString &peerDeviceId) const
{
    QJsonArray result;
    QSqlQuery q(m_db);

    if (peerDeviceId.isEmpty()) {
        // Fallback (unbekannter Peer): alle lokalen pending-Einträge
        q.prepare(
            "SELECT id, table_name, row_id, operation, timestamp_ms, device_id, local_seq "
            "FROM sync_log "
            "WHERE direction = 'local' AND status = 'pending' "
            "ORDER BY device_id, local_seq ASC");
    } else {
        // Knowledge Matrix: nur was der Peer noch nicht hat
        // Für jede origin_device_id: local_seq > sync_knowledge[peer][origin]
        q.prepare(
            "SELECT sl.id, sl.table_name, sl.row_id, sl.operation, "
            "       sl.timestamp_ms, sl.device_id, sl.local_seq "
            "FROM sync_log sl "
            "WHERE sl.direction = 'local' "
            "AND sl.local_seq > COALESCE("
            "  (SELECT max_seq FROM sync_knowledge "
            "   WHERE peer_device_id = :peer AND origin_device_id = sl.device_id),"
            "  0) "
            "ORDER BY sl.device_id, sl.local_seq ASC");
        q.bindValue(":peer", peerDeviceId);
    }

    if (!q.exec()) {
        qWarning() << "[DatabaseSync] exportChanges:" << q.lastError().text();
        return result;
    }

    while (q.next()) {
        const int     logId     = q.value(0).toInt();
        const QString table     = q.value(1).toString();
        const int     rowId     = q.value(2).toInt();
        const QString operation = q.value(3).toString();
        const qint64  ts        = q.value(4).toLongLong();
        const QString devId     = q.value(5).toString();
        const int     localSeq  = q.value(6).toInt();

        QJsonObject obj;
        obj["log_id"]    = logId;
        obj["table"]     = table;
        obj["row_id"]    = rowId;
        obj["operation"] = operation;
        obj["timestamp"] = ts;
        obj["device_id"] = devId;
        obj["local_seq"] = localSeq;

        if (operation == "DELETE") {
            obj["data"] = QJsonObject();
        } else {
            QJsonObject rowData = rowToJson(table, rowId);
            if (rowData.isEmpty()) {
                // Zeile existiert nicht mehr
                obj["operation"] = QString("DELETE");
                obj["data"]      = QJsonObject();
            } else if (rowData.value("is_deleted").toInt(0) == 1) {
                // Soft Delete: UPDATE-Trigger hat den Tombstone geloggt,
                // Empfänger soll aber applyDelete() ausführen (is_deleted=1 setzen)
                obj["operation"] = QString("DELETE");
                obj["data"]      = QJsonObject();
            } else {
                obj["data"] = rowData;
            }
        }
        result.append(obj);
    }

    return result;
}

// ── Anwenden empfangener Änderungen ──────────────────────────────────────

bool DatabaseSync::applyChanges(const QJsonArray &changes)
{
    int applied = 0, skipped = 0;

    // Höchste Seq pro Ursprungsgerät für Knowledge-Matrix-Update sammeln
    QHash<QString, int> maxSeqByOrigin;

    // FK-Constraints deaktivieren (wie applyFullExport) – verhindert FK-Fehler
    // wenn Fahrten vor ihren Adressen/Fahrern ankommen.
    // SQLite ignoriert PRAGMA foreign_keys innerhalb einer offenen Transaktion,
    // daher außerhalb setzen.
    { QSqlQuery fkOff(m_db); fkOff.exec("PRAGMA foreign_keys = OFF"); }

    // Änderungen nach Tabellen-Priorität sortieren: fahrer → adressen → fahrten
    // Verhindert FOREIGN KEY-Fehler wenn eine Fahrt vor ihrer Adresse/Fahrer ankommt
    auto tablePriority = [](const QString &t) -> int {
        if (t == "fahrer")   return 0;
        if (t == "adressen") return 1;
        return 2; // fahrten
    };
    QList<QJsonObject> sorted;
    sorted.reserve(changes.size());
    for (const QJsonValue &v : changes)
        sorted.append(v.toObject());
    std::stable_sort(sorted.begin(), sorted.end(),
        [&tablePriority](const QJsonObject &a, const QJsonObject &b) {
            return tablePriority(a["table"].toString()) < tablePriority(b["table"].toString());
        });

    for (const QJsonObject &c : sorted) {
        const QString  table  = c["table"].toString();
        const int      rowId  = c["row_id"].toInt();
        const QString  op     = c["operation"].toString();
        const qint64   ts     = c["timestamp"].toVariant().toLongLong();
        const QString  devId  = c["device_id"].toString();
        const int      lSeq   = c["local_seq"].toInt(0);
        const QJsonObject data = c["data"].toObject();

        if (!kSyncTables.contains(table)) { skipped++; continue; }
        if (devId == m_deviceId)          { skipped++; continue; }

        // Duplikat?
        {
            QSqlQuery dupQ(m_db);
            dupQ.prepare(
                "SELECT 1 FROM sync_log "
                "WHERE table_name=? AND row_id=? AND timestamp_ms=? AND device_id=?");
            dupQ.addBindValue(table);
            dupQ.addBindValue(rowId);
            dupQ.addBindValue(ts);
            dupQ.addBindValue(devId);
            dupQ.exec();
            if (dupQ.next()) { skipped++; continue; }
        }

        // Konflikt: lokaler Eintrag neuer?
        {
            QSqlQuery localQ(m_db);
            localQ.prepare(
                "SELECT timestamp_ms, device_id FROM sync_log "
                "WHERE table_name=? AND row_id=? "
                "ORDER BY timestamp_ms DESC, device_id ASC LIMIT 1");
            localQ.addBindValue(table);
            localQ.addBindValue(rowId);
            localQ.exec();
            if (localQ.next()) {
                qint64  localTs  = localQ.value(0).toLongLong();
                QString localDev = localQ.value(1).toString();
                if (localTs > ts) { skipped++; continue; }
                if (localTs == ts && localDev < devId) { skipped++; continue; }
            }
        }

        bool ok = false;
        {
            QSqlQuery flagOn(m_db);
            flagOn.exec("INSERT OR REPLACE INTO sync_meta(key,value) VALUES('applying_remote','1')");

            ok = (op == "DELETE") ? applyDelete(table, rowId)
                                  : applyUpsert(table, rowId, data);

            QSqlQuery flagOff(m_db);
            flagOff.exec("INSERT OR REPLACE INTO sync_meta(key,value) VALUES('applying_remote','0')");
        }
        if (!ok) { skipped++; continue; }

        // Als empfangene Änderung im sync_log vermerken
        {
            const QByteArray payloadJson =
                QJsonDocument(data).toJson(QJsonDocument::Compact);
            QSqlQuery logQ(m_db);
            logQ.prepare(
                "INSERT OR IGNORE INTO sync_log"
                "  (table_name,row_id,operation,timestamp_ms,device_id,"
                "   direction,status,payload,local_seq) "
                "VALUES(?,?,?,?,?,'remote','received',?,?)");
            logQ.addBindValue(table);
            logQ.addBindValue(rowId);
            logQ.addBindValue(op);
            logQ.addBindValue(ts);
            logQ.addBindValue(devId);
            logQ.addBindValue(QString::fromUtf8(payloadJson));
            logQ.addBindValue(lSeq);
            logQ.exec();
        }

        // Lokale Einträge für diese Row als "sent" markieren
        {
            QSqlQuery sentQ(m_db);
            sentQ.prepare(
                "UPDATE sync_log SET status='sent' "
                "WHERE table_name=? AND row_id=? AND direction='local'");
            sentQ.addBindValue(table);
            sentQ.addBindValue(rowId);
            sentQ.exec();
        }

        // Höchste Seq pro Origin-Gerät merken
        if (lSeq > 0)
            maxSeqByOrigin[devId] = qMax(maxSeqByOrigin.value(devId, 0), lSeq);

        applied++;
        emit changeApplied(table, rowId);
    }

    // Knowledge Matrix: eigenen Empfangsstand aktualisieren
    // (peer_device_id = m_deviceId = ich selbst; bedeutet "ich habe von origin bis seq")
    for (auto it = maxSeqByOrigin.constBegin(); it != maxSeqByOrigin.constEnd(); ++it)
        updateKnowledge(m_deviceId, it.key(), it.value());

    // FK-Constraints wieder aktivieren
    { QSqlQuery fkOn(m_db); fkOn.exec("PRAGMA foreign_keys = ON"); }

    qDebug() << "[DatabaseSync] applyChanges: angewendet=" << applied
             << "übersprungen=" << skipped;
    return true;
}

// ── Knowledge Matrix nach erfolgreichem Senden ────────────────────────────

void DatabaseSync::markAsSent(const QString &peerDeviceId,
                               const QJsonArray &sentChanges)
{
    if (peerDeviceId.isEmpty()) {
        qDebug() << "[DatabaseSync] markAsSent: kein peerDeviceId – nur status update";
    }

    // Knowledge Matrix und status='sent' aktualisieren
    QHash<QString, int> maxSeqByOrigin;

    for (const QJsonValue &v : sentChanges) {
        const QJsonObject c = v.toObject();
        const QString table = c["table"].toString();
        const int     rowId = c["row_id"].toInt();
        const qint64  ts    = c["timestamp"].toVariant().toLongLong();
        const QString devId = c["device_id"].toString();
        const int     lSeq  = c["local_seq"].toInt(0);

        // status in sync_log auf 'sent' setzen (für SyncLogView-Anzeige)
        QSqlQuery q(m_db);
        q.prepare(
            "UPDATE sync_log SET status='sent' "
            "WHERE table_name=? AND row_id=? AND timestamp_ms=? "
            "AND direction='local' AND status='pending'");
        q.addBindValue(table);
        q.addBindValue(rowId);
        q.addBindValue(ts);
        q.exec();

        // Höchste Seq pro Origin merken
        if (lSeq > 0 && !devId.isEmpty())
            maxSeqByOrigin[devId] = qMax(maxSeqByOrigin.value(devId, 0), lSeq);
    }

    // Knowledge Matrix update
    if (!peerDeviceId.isEmpty()) {
        for (auto it = maxSeqByOrigin.constBegin(); it != maxSeqByOrigin.constEnd(); ++it) {
            updateKnowledge(peerDeviceId, it.key(), it.value());
            qDebug() << "[DatabaseSync] Knowledge:" << peerDeviceId.left(8)
                     << "hat von" << it.key().left(8)
                     << "jetzt bis Seq" << it.value();
        }
    }

    qDebug() << "[DatabaseSync] markAsSent für Peer"
             << (peerDeviceId.isEmpty() ? "(unbekannt)" : peerDeviceId.left(8))
             << "– Einträge:" << sentChanges.size();
}

// Wenn der Peer per SYNC_REQUEST anzeigt, dass er alles hat (exportChanges=0),
// aktualisieren wir die Knowledge-Matrix trotzdem auf unsere aktuelle max_seq.
// Sonst bleibt die Matrix auf dem alten Stand und zeigt fälschlich "X fehlen".
void DatabaseSync::updateKnowledgeForPeer(const QString &peerDeviceId)
{
    if (peerDeviceId.isEmpty()) return;

    // Aktuelle max local_seq pro origin_device_id aus sync_log ermitteln
    QSqlQuery q(m_db);
    q.exec("SELECT device_id, MAX(local_seq) FROM sync_log "
           "WHERE direction='local' GROUP BY device_id");
    while (q.next()) {
        const QString originId = q.value(0).toString();
        const int     maxSeq   = q.value(1).toInt();
        if (!originId.isEmpty() && maxSeq > 0) {
            updateKnowledge(peerDeviceId, originId, maxSeq);
            qDebug() << "[DatabaseSync] updateKnowledgeForPeer:"
                     << peerDeviceId.left(8)
                     << "hat von" << originId.left(8)
                     << "bis Seq" << maxSeq;
        }
    }
}



bool DatabaseSync::applyUpsert(const QString &table, int /*rowId*/,
                                const QJsonObject &data)
{
    if (data.isEmpty()) return false;

    const QStringList cols = data.keys();
    QStringList placeholders(cols.size(), "?");

    QSqlQuery q(m_db);
    q.prepare(QString("INSERT OR REPLACE INTO %1 (%2) VALUES (%3)")
              .arg(table, cols.join(","), placeholders.join(",")));

    for (const QString &col : cols) {
        const QJsonValue val = data[col];
        if (val.isNull() || val.isUndefined())
            q.addBindValue(QVariant());
        else if (val.isBool())
            q.addBindValue(val.toBool() ? 1 : 0);
        else if (val.isDouble())
            q.addBindValue(val.toDouble());
        else
            q.addBindValue(val.toString());
    }

    if (!q.exec()) {
        qWarning() << "[DatabaseSync] applyUpsert" << table << ":" << q.lastError().text();
        return false;
    }
    return true;
}

bool DatabaseSync::applyDelete(const QString &table, int rowId)
{
    // Soft Delete: Tombstone setzen statt physisch löschen.
    // Löscht der Sync-Partner einen Datensatz, wird is_deleted=1 gesetzt.
    // Die Zeile bleibt erhalten damit weitere Peers via Relay informiert werden können.
    QSqlQuery q(m_db);
    q.prepare(QString("UPDATE %1 SET is_deleted=1 WHERE id=?").arg(table));
    q.addBindValue(rowId);
    if (!q.exec()) {
        qWarning() << "[DatabaseSync] applyDelete (soft)" << table << rowId
                   << ":" << q.lastError().text();
        return false;
    }
    // UPDATE trifft 0 Zeilen wenn rowId nicht existiert – das ist OK,
    // der Datensatz war möglicherweise nie auf diesem Gerät vorhanden.
    return true;
}

// ── Relay: empfangene Änderungen für Weiterleitung eintragen ─────────────

bool DatabaseSync::relayChanges(const QJsonArray &changes)
{
    if (changes.isEmpty()) return true;

    int count = 0;
    for (const QJsonValue &v : changes) {
        const QJsonObject obj = v.toObject();

        const QString table  = obj.value("table").toString();
        const int     rowId  = obj.value("row_id").toInt();
        const QString op     = obj.value("operation").toString();
        const qint64  ts     = obj.value("timestamp").toVariant().toLongLong();
        const QString devId  = obj.value("device_id").toString();
        const int     lSeq   = obj.value("local_seq").toInt(0);
        const QJsonObject data = obj.value("data").toObject();

        if (devId == m_deviceId) continue;
        if (!kSyncTables.contains(table)) continue;

        // Duplikat-Check: schon als local/pending mit dieser Seq vorhanden?
        {
            QSqlQuery dup(m_db);
            dup.prepare(
                "SELECT 1 FROM sync_log "
                "WHERE table_name=? AND row_id=? AND timestamp_ms=? "
                "  AND device_id=? AND direction='local'");
            dup.addBindValue(table);
            dup.addBindValue(rowId);
            dup.addBindValue(ts);
            dup.addBindValue(devId);
            dup.exec();
            if (dup.next()) continue;
        }

        const QByteArray payload =
            QJsonDocument(data).toJson(QJsonDocument::Compact);

        QSqlQuery ins(m_db);
        ins.prepare(
            "INSERT OR IGNORE INTO sync_log"
            "  (table_name, row_id, operation, timestamp_ms, device_id,"
            "   direction, status, payload, local_seq)"
            " VALUES(?, ?, ?, ?, ?, 'local', 'pending', ?, ?)");
        ins.addBindValue(table);
        ins.addBindValue(rowId);
        ins.addBindValue(op);
        ins.addBindValue(ts);
        ins.addBindValue(devId);
        ins.addBindValue(QString::fromUtf8(payload));
        ins.addBindValue(lSeq);

        if (!ins.exec())
            qWarning() << "[DatabaseSync] relayChanges Fehler:" << ins.lastError().text();
        else if (ins.numRowsAffected() > 0)
            ++count;
        // numRowsAffected() == 0 → IGNORE wegen Duplikat, kein Fehler
    }

    qDebug() << "[DatabaseSync] relayChanges: eingetragen für Weiterleitung:" << count;
    return true;
}

// ── Protokoll-Abfrage ─────────────────────────────────────────────────────

bool DatabaseSync::isNewDevice() const
{
    // Echte Nutzdaten = Zeilen in Fahrten/Adressen/Fahrer die nicht soft-gelöscht sind.
    // Migrations-Einträge (timestamp_ms=1) zählen nicht – die entstehen auch bei
    // einer frisch initialisierten leeren DB.
    // Der Default-Fahrer "unbekannt" (is_default=1) zählt ebenfalls nicht –
    // er wird bei jeder frischen DB automatisch angelegt.
    QSqlQuery qFahrer(m_db);
    qFahrer.exec("SELECT COUNT(*) FROM fahrer WHERE is_deleted=0 AND is_default=0");
    if (qFahrer.next() && qFahrer.value(0).toInt() > 0) return false;

    for (const QString &table : {QString("adressen"), QString("fahrten")}) {
        QSqlQuery q(m_db);
        q.exec(QString("SELECT COUNT(*) FROM %1 WHERE is_deleted=0").arg(table));
        if (q.next() && q.value(0).toInt() > 0)
            return false;
    }
    return true;
}

bool DatabaseSync::hasPendingForAnyKnownPeer() const
{
    // Gibt true zurück wenn sync_log Einträge hat die noch nicht an mindestens
    // einen bekannten Peer gesendet wurden (d.h. max_seq in sync_knowledge < local_seq).
    QSqlQuery q(m_db);
    q.exec(
        "SELECT COUNT(*) FROM sync_log sl "
        "WHERE sl.direction = 'local' "
        "AND sl.local_seq > COALESCE("
        "  (SELECT MIN(sk.max_seq) FROM sync_knowledge sk "
        "   WHERE sk.origin_device_id = sl.device_id "
        "   AND sk.peer_device_id != sl.device_id), "
        "  0)"
    );
    if (q.next()) return q.value(0).toInt() > 0;
    return false;
}

QList<SyncLogEntry> DatabaseSync::getSyncLog(int limit) const
{
    QList<SyncLogEntry> result;
    QSqlQuery q(m_db);
    // timestamp_ms <= 2 sind Migrations-Sentinels (01.01.1970) – im Log ausblenden.
    // Sie erscheinen in der Summary als eigener Zähler.
    q.prepare(
        "SELECT id, table_name, row_id, operation, timestamp_ms, "
        "       device_id, direction, status, local_seq "
        "FROM sync_log "
        "WHERE timestamp_ms > 2 "
        "ORDER BY timestamp_ms DESC "
        "LIMIT ?");
    q.addBindValue(limit);

    if (!q.exec()) {
        qWarning() << "[DatabaseSync] getSyncLog:" << q.lastError().text();
        return result;
    }

    while (q.next()) {
        SyncLogEntry e;
        e.id        = q.value(0).toInt();
        e.tableName = q.value(1).toString();
        e.rowId     = q.value(2).toInt();
        e.operation = q.value(3).toString();
        e.timestamp = QDateTime::fromMSecsSinceEpoch(q.value(4).toLongLong());
        e.deviceId  = q.value(5).toString();
        e.direction = q.value(6).toString();
        e.status    = q.value(7).toString();
        e.localSeq  = q.value(8).toInt();
        e.deviceName = peerDisplayName(e.deviceId);
        result.append(e);
    }
    return result;
}

int DatabaseSync::migrationEntryCount() const
{
    QSqlQuery q(m_db);
    if (q.exec("SELECT COUNT(*) FROM sync_log WHERE timestamp_ms <= 2") && q.next())
        return q.value(0).toInt();
    return 0;
}

QList<KnowledgeEntry> DatabaseSync::getKnowledgeMatrix() const
{
    QList<KnowledgeEntry> result;

    // Hilfsfunktion: Max-Seq die ein bestimmtes Ursprungsgerät erzeugt hat.
    //
    // Eigenes Gerät: nur direction='local' → nur selbst erzeugte Einträge.
    // Fremdes Gerät: KEIN direction-Filter → empfangene Einträge (direction='remote')
    //   UND weitergeleitete (direction='local' via relay) werden berücksichtigt.
    //   So ergibt sich die tatsächlich höchste bekannte Seq des Fremdgeräts,
    //   unabhängig davon ob sie direkt oder über Relay ankam.
    auto originMax = [&](const QString &originDevId) -> int {
        QSqlQuery q2(m_db);
        if (originDevId == m_deviceId) {
            // Eigene Einträge: nur selbst erzeugte (direction='local')
            q2.prepare("SELECT COALESCE(MAX(local_seq),0) FROM sync_log "
                       "WHERE device_id=? AND direction='local'");
        } else {
            // Fremdes Gerät: alle Einträge (remote empfangen + lokal relayt)
            q2.prepare("SELECT COALESCE(MAX(local_seq),0) FROM sync_log "
                       "WHERE device_id=?");
        }
        q2.addBindValue(originDevId);
        if (q2.exec() && q2.next()) return q2.value(0).toInt();
        return 0;
    };

    // Alle Einträge aus sync_knowledge
    QSqlQuery q(m_db);
    q.exec("SELECT peer_device_id, origin_device_id, max_seq "
           "FROM sync_knowledge ORDER BY peer_device_id, origin_device_id");
    while (q.next()) {
        KnowledgeEntry e;
        e.peerDeviceId   = q.value(0).toString();
        e.originDeviceId = q.value(1).toString();

        // Eintrag peer==origin für Fremdgeräte überspringen:
        // Jedes Gerät kennt seine eigenen Änderungen immer vollständig –
        // dieser Eintrag entsteht fehlerhaft wenn relay-Daten an den Ursprung
        // zurückgesendet werden und ist semantisch bedeutungslos.
        if (e.peerDeviceId == e.originDeviceId && e.peerDeviceId != m_deviceId)
            continue;

        e.peerName       = peerDisplayNameShort(e.peerDeviceId);
        e.originName     = peerDisplayNameShort(e.originDeviceId);
        e.maxSeq         = q.value(2).toInt();
        e.originMaxSeq   = qMax(originMax(e.originDeviceId), e.maxSeq);
        e.pendingCount   = e.originMaxSeq - e.maxSeq;  // immer >= 0 durch qMax oben
        e.isSelf         = (e.peerDeviceId == m_deviceId &&
                            e.originDeviceId == m_deviceId);
        e.isOwnDevice    = (e.peerDeviceId == m_deviceId);
        result.append(e);
    }

    // Synthetischer Eintrag: eigener aktueller Stand (falls noch nicht in sync_knowledge)
    {
        const int ownMax = originMax(m_deviceId);
        bool found = false;
        for (const auto &e : result)
            if (e.peerDeviceId == m_deviceId && e.originDeviceId == m_deviceId)
            { found = true; break; }
        if (!found) {
            KnowledgeEntry self;
            self.peerDeviceId   = m_deviceId;
            self.peerName       = peerDisplayNameShort(m_deviceId);
            self.originDeviceId = m_deviceId;
            self.originName     = peerDisplayNameShort(m_deviceId);
            self.maxSeq         = ownMax;
            self.originMaxSeq   = ownMax;
            self.pendingCount   = 0;
            self.isSelf         = true;
            self.isOwnDevice    = true;
            result.prepend(self);
        }
    }

    return result;
}

int DatabaseSync::maxLocalSeq() const
{
    QSqlQuery q(m_db);
    q.prepare("SELECT COALESCE(MAX(local_seq),0) FROM sync_log "
              "WHERE device_id=? AND direction='local'");
    q.addBindValue(m_deviceId);
    if (q.exec() && q.next())
        return q.value(0).toInt();
    return 0;
}

void DatabaseSync::setPeerName(const QString &deviceId, const QString &name)
{
    if (deviceId.isEmpty() || name.isEmpty()) return;
    QSqlQuery q(m_db);
    q.prepare("INSERT OR REPLACE INTO sync_meta(key,value) VALUES(?,?)");
    q.addBindValue(QStringLiteral("peer_name_") + deviceId);
    q.addBindValue(name);
    q.exec();
}

QString DatabaseSync::peerDisplayName(const QString &deviceId) const
{
    // Eigenes Gerät: gespeicherten Namen verwenden + "(lokal)" Suffix
    // So ist jedes Gerät eindeutig benannt, auch wenn zwei Einträge
    // "Dieses Gerät" zeigen würden – man sieht jetzt den echten Namen.
    QSqlQuery q(m_db);
    q.prepare("SELECT value FROM sync_meta WHERE key=?");
    q.addBindValue(QStringLiteral("peer_name_") + deviceId);
    if (q.exec() && q.next()) {
        const QString name = q.value(0).toString();
        if (!name.isEmpty()) {
            if (deviceId == m_deviceId)
                return name + QStringLiteral(" \u24c1");  // Ⓛ = lokal
            return name;
        }
    }
    if (deviceId == m_deviceId)
        return QStringLiteral("Dieses Ger\u00e4t \u24c1");
    // Kein Name bekannt → leeren String zurückgeben.
    // Aufrufer (getSyncLog) zeigt dann "unbekannt (XXXXXXXX)" in grau.
    // Aufrufer (getKnowledgeMatrix) hat eigene Fallback-Logik.
    return QString();
}

QString DatabaseSync::peerDisplayNameShort(const QString &deviceId) const
{
    const QString name = peerDisplayName(deviceId);
    if (!name.isEmpty()) return name;
    return deviceId.left(8) + QStringLiteral("\u2026");  // z.B. "4870c578…"
}

void DatabaseSync::pruneLog(int daysOld)
{
    const qint64 cutoff =
        QDateTime::currentMSecsSinceEpoch() - (qint64)daysOld * 86400 * 1000;
    QSqlQuery q(m_db);
    // timestamp_ms > 2: schützt den Migrations-Sentinel (ts=1)
    q.prepare("DELETE FROM sync_log WHERE timestamp_ms > 2 AND timestamp_ms < ?");
    q.addBindValue(cutoff);
    q.exec();
    qDebug() << "[DatabaseSync] pruneLog: gelöscht" << q.numRowsAffected()
             << "Einträge älter als" << daysOld << "Tage";

    // Alte Knowledge-Matrix-Einträge für nicht mehr bekannte Geräte können
    // stehen bleiben – sie sind harmlos und klein.
}

// ── Hilfsfunktionen ───────────────────────────────────────────────────────

QStringList DatabaseSync::columnNames(const QString &tableName) const
{
    QStringList cols;
    QSqlQuery q(m_db);
    q.exec(QString("PRAGMA table_info(%1)").arg(tableName));
    while (q.next())
        cols << q.value("name").toString();
    return cols;
}

QJsonObject DatabaseSync::rowToJson(const QString &tableName, int rowId) const
{
    QSqlQuery q(m_db);
    q.prepare(QString("SELECT * FROM %1 WHERE id=?").arg(tableName));
    q.addBindValue(rowId);
    if (!q.exec() || !q.next()) return {};

    QJsonObject obj;
    const QSqlRecord rec = q.record();
    for (int i = 0; i < rec.count(); ++i)
        obj[rec.fieldName(i)] = QJsonValue::fromVariant(q.value(i));
    return obj;
}

// ── Vollständiger Export (Erstabgleich) ───────────────────────────────────

QJsonObject DatabaseSync::fullExport() const
{
    QJsonObject snapshot;
    snapshot["full_export"] = true;
    snapshot["device_id"]   = m_deviceId;
    snapshot["timestamp"]   = QDateTime::currentMSecsSinceEpoch();

    QJsonObject tables;
    for (const QString &table : kSyncTables) {
        QJsonArray rows;
        QSqlQuery q(m_db);
        q.exec(QString("SELECT * FROM %1").arg(table));
        const QSqlRecord rec = q.record();
        while (q.next()) {
            QJsonObject row;
            for (int i = 0; i < rec.count(); ++i)
                row[rec.fieldName(i)] = QJsonValue::fromVariant(q.value(i));
            rows.append(row);
        }
        tables[table] = rows;
        qDebug() << "[DatabaseSync] fullExport:" << table << rows.size() << "Rows";
    }
    snapshot["tables"] = tables;
    // Eigene aktuelle max_seq mitschicken damit der Empfänger die Knowledge Matrix
    // korrekt befüllen kann (statt der falschen Migration-Row-Anzahl zu verwenden).
    snapshot["sender_max_seq"] = maxLocalSeq();
    return snapshot;
}

bool DatabaseSync::applyFullExport(const QJsonObject &snapshot)
{
    if (!snapshot["full_export"].toBool()) {
        qWarning() << "[DatabaseSync] applyFullExport: kein full_export-Flag";
        return false;
    }

    const QString senderDeviceId = snapshot["device_id"].toString();
    if (senderDeviceId == m_deviceId) {
        qWarning() << "[DatabaseSync] applyFullExport: eigener Snapshot ignoriert";
        return false;
    }

    qDebug() << "[DatabaseSync] applyFullExport von" << senderDeviceId;

    // Foreign-Key-Checks VOR der Transaktion deaktivieren –
    // SQLite ignoriert PRAGMA foreign_keys innerhalb einer offenen Transaktion.
    // kSyncTables ist zwar in der richtigen Reihenfolge (fahrer→adressen→fahrten),
    // aber OFF ist sicherer gegen künftige Schema-Änderungen.
    { QSqlQuery fkOff(m_db); fkOff.exec("PRAGMA foreign_keys = OFF"); }

    // applying_remote setzen: Trigger sollen während des Imports nicht feuern.
    // Ohne Flag würden INSERT OR REPLACE-Trigger sync_log-Einträge mit unserer
    // eigenen device_id erzeugen, die dann sofort wieder gelöscht werden –
    // unnötiger Overhead und kurzzeitig falscher Log-Zustand.
    { QSqlQuery flagOn(m_db);
      flagOn.exec("INSERT OR REPLACE INTO sync_meta(key,value) VALUES('applying_remote','1')"); }

    m_db.transaction();

    const QJsonObject tables = snapshot["tables"].toObject();
    for (const QString &table : kSyncTables) {
        if (!tables.contains(table)) continue;

        const QJsonArray rows = tables[table].toArray();
        for (const QJsonValue &v : rows) {
            const QJsonObject row = v.toObject();
            const int rowId = row["id"].toInt();
            if (!applyUpsert(table, rowId, row)) {
                m_db.rollback();
                // applying_remote und FK zurücksetzen bevor wir abbrechen
                { QSqlQuery f(m_db); f.exec("INSERT OR REPLACE INTO sync_meta(key,value) VALUES('applying_remote','0')"); }
                { QSqlQuery f(m_db); f.exec("PRAGMA foreign_keys = ON"); }
                return false;
            }
        }
        qDebug() << "[DatabaseSync] applyFullExport:" << table
                 << rows.size() << "Rows übernommen";
    }

    // sync_log + Knowledge Matrix zurücksetzen und neu migrieren
    {
        QSqlQuery q(m_db);
        q.exec("DELETE FROM sync_log");
        q.exec("DELETE FROM sync_meta WHERE key='migrated'");
        q.exec("DELETE FROM sync_knowledge");  // sauberer Startpunkt
    }

    m_db.commit();

    // applying_remote zurücksetzen + FK wieder aktivieren
    { QSqlQuery flagOff(m_db);
      flagOff.exec("INSERT OR REPLACE INTO sync_meta(key,value) VALUES('applying_remote','0')"); }
    { QSqlQuery fkOn(m_db); fkOn.exec("PRAGMA foreign_keys = ON"); }

    migrateExistingData();

    // Knowledge Matrix: Sender des fullExports eintragen.
    // WICHTIG: senderSeq kommt aus dem Snapshot-Feld "sender_max_seq" (= Sender's
    // echter lokaler max_seq). Früher wurde MAX(local_seq) der eigenen Migration
    // verwendet – das war die Anzahl importierter Rows, NICHT die Seq des Senders.
    // Beispiel: Sender hat 47 lokale Seqs, aber nur 39 Rows → Migration ergibt 39
    // → fälschlich "bis Seq 39" eingetragen → Sender schickt beim nächsten Treffen
    // unnötigerweise Seqs 40..47 erneut.
    {
        const int senderSeq = snapshot["sender_max_seq"].toInt(0);
        if (senderSeq > 0) {
            QSqlQuery ins(m_db);
            ins.prepare("INSERT OR REPLACE INTO sync_knowledge"
                        "(peer_device_id, origin_device_id, max_seq)"
                        " VALUES(?, ?, ?)");
            ins.addBindValue(m_deviceId);     // peer = dieses Gerät
            ins.addBindValue(senderDeviceId); // origin = Sender des fullExports
            ins.addBindValue(senderSeq);
            ins.exec();
            qDebug() << "[DatabaseSync] applyFullExport: Knowledge eingetragen –"
                     << "Sender" << senderDeviceId.left(8)
                     << "bis Seq" << senderSeq << "(aus Snapshot)";
        } else {
            // Fallback für alte Snapshots (ohne sender_max_seq): eigene Migration-Seqs
            QSqlQuery seqQ(m_db);
            seqQ.exec("SELECT COALESCE(MAX(local_seq),0) FROM sync_log WHERE direction='local'");
            const int fallbackSeq = (seqQ.next()) ? seqQ.value(0).toInt() : 0;
            if (fallbackSeq > 0) {
                QSqlQuery ins(m_db);
                ins.prepare("INSERT OR REPLACE INTO sync_knowledge"
                            "(peer_device_id, origin_device_id, max_seq)"
                            " VALUES(?, ?, ?)");
                ins.addBindValue(m_deviceId);
                ins.addBindValue(senderDeviceId);
                ins.addBindValue(fallbackSeq);
                ins.exec();
                qDebug() << "[DatabaseSync] applyFullExport: Knowledge (Fallback) –"
                         << "Sender" << senderDeviceId.left(8)
                         << "bis Seq" << fallbackSeq;
            }
        }
    }

    qDebug() << "[DatabaseSync] applyFullExport abgeschlossen";
    return true;
}
