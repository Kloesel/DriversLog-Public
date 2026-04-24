#pragma once
#include <QObject>
#include <QSqlDatabase>
#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>
#include <QDateTime>

// ── Einzelner Eintrag im Sync-Protokoll ─────────────────────────────────────
struct SyncLogEntry {
    int      id;
    QString  tableName;
    int      rowId;
    QString  operation;   // "INSERT" | "UPDATE" | "DELETE"
    QDateTime timestamp;
    QString  deviceId;
    QString  deviceName;  // Anzeigename des Geräts (aus sync_meta)
    QString  direction;   // "local" | "remote"
    QString  status;      // "pending" | "sent" | "received"
    int      localSeq;
};

// ── Eintrag in der Knowledge Matrix ─────────────────────────────────────────
struct KnowledgeEntry {
    QString peerDeviceId;
    QString peerName;        // Anzeigename des Peer-Geräts (immer echter Name)
    QString originDeviceId;
    QString originName;      // Anzeigename des Ursprungsgeräts (immer echter Name)
    int     maxSeq;          // Höchste Seq die Peer von Origin empfangen hat
    int     originMaxSeq;    // Tatsächliche aktuelle Max-Seq des Ursprungsgeräts
    int     pendingCount;    // = originMaxSeq - maxSeq (0 = vollständig synchronisiert)
    bool    isSelf;          // peer == origin == dieses Gerät
    bool    isOwnDevice;     // peer == dieses Gerät (für Hervorhebung)
};

/**
 * DatabaseSync – SQLite-Trigger-basiertes Change-Log für WLAN-Sync.
 *
 * WICHTIG: initializeSync() muss beim App-Start aufgerufen werden –
 * unabhängig davon ob WLAN-Sync aktiviert ist.
 *
 * Knowledge Matrix (sync_knowledge):
 *   Für jeden bekannten Peer (peer_device_id) und jedes Ursprungsgerät
 *   (origin_device_id) wird die höchste gesendete Sequenznummer gespeichert.
 *   exportChanges(peerDeviceId) liefert nur Einträge die der Peer noch
 *   nicht hat – auch nach Reconnect oder längerer Offline-Phase gehen
 *   keine Änderungen verloren.
 *
 * Sequenznummern (local_seq):
 *   Jede Änderung erhält eine pro-Gerät monoton steigende Sequenznummer.
 *   Eigene Änderungen: vom SQLite-Trigger vergeben.
 *   Relay-Änderungen: original local_seq des Ursprungsgeräts übernommen.
 *
 * Relay-Mechanismus:
 *   relayChanges() trägt empfangene Änderungen als 'local/pending' ein,
 *   damit sie an Geräte weitergeleitet werden die noch nicht mit dem
 *   Ursprungsgerät kommuniziert haben.
 *   Endlosschleifen sind ausgeschlossen:
 *   - applyChanges() überspringt Einträge mit eigener device_id
 *   - relayChanges() überspringt Einträge mit eigener device_id
 *   - Duplikat-Check verhindert mehrfaches Eintragen
 */
class DatabaseSync : public QObject
{
    Q_OBJECT
public:
    explicit DatabaseSync(QSqlDatabase db,
                          const QString &deviceId,
                          QObject *parent = nullptr);

    /**
     * Beim App-Start aufrufen – legt Tabellen + Trigger an (idempotent),
     * führt beim allerersten Aufruf einmalig die Datenmigration durch.
     */
    bool initializeSync();

    /**
     * Exportiert alle Änderungen die der angegebene Peer noch nicht hat.
     * Nutzt die Knowledge Matrix (sync_knowledge) für effizientes Delta.
     *
     * Falls peerDeviceId leer ist (unbekannter Peer): Fallback auf alle
     * 'local/pending'-Einträge (Rückwärtskompatibilität).
     *
     * Format: { table, row_id, operation, timestamp, device_id, local_seq, data:{} }
     */
    QJsonArray exportChanges(const QString &peerDeviceId) const;

    /**
     * Wendet empfangene Änderungen an (last-timestamp-wins).
     * Aktualisiert die eigene Knowledge Matrix für empfangene Seqs.
     */
    bool applyChanges(const QJsonArray &changes);

    /**
     * Trägt empfangene Änderungen als 'local/pending' ein, damit sie
     * an andere Peers weitergeleitet werden können (Relay-Mechanismus).
     * Eigene Änderungen und bereits vorhandene Einträge werden übersprungen.
     * Bewahrt die original local_seq des Ursprungsgeräts.
     */
    bool relayChanges(const QJsonArray &changes);

    /**
     * Aktualisiert die Knowledge Matrix nach erfolgreichem Senden:
     * Speichert für jeden (peer, origin) die höchste gesendete Seq.
     * Markiert Einträge zusätzlich als 'sent' im sync_log (für SyncLogView).
     */
    void markAsSent(const QString &peerDeviceId, const QJsonArray &sentChanges);
    void updateKnowledgeForPeer(const QString &peerDeviceId);

    /**
     * Exportiert ALLE Rows als vollständigen Snapshot (Erstabgleich).
     */
    QJsonObject fullExport() const;

    /**
     * Wendet einen vollständigen Snapshot an (Erstabgleich).
     */
    bool applyFullExport(const QJsonObject &snapshot);

    /**
     * Gibt true zurück wenn dieses Gerät noch keine Nutzdaten hat (leere DB).
     * Kriterium: keine Fahrten/Adressen/Fahrer außer Migrationsdaten (ts=1).
     * Wird beim Start geprüft – neues Gerät fordert dann fullExport statt Delta.
     */
    bool isNewDevice() const;
    bool hasPendingForAnyKnownPeer() const;

    QList<SyncLogEntry> getSyncLog(int limit = 200) const;

    /**
     * Gibt die Anzahl der Migrations-Einträge zurück (timestamp_ms <= 2).
     * Diese werden in getSyncLog ausgeblendet und nur in der Summary gezählt.
     */
    int migrationEntryCount() const;

    /**
     * Gibt die vollständige Knowledge Matrix zurück (alle Einträge in sync_knowledge).
     * Enthält zusätzlich einen synthetischen Eintrag für den eigenen Gerätstand
     * (peer = self, origin = self, max_seq = höchste eigene local_seq).
     */
    QList<KnowledgeEntry> getKnowledgeMatrix() const;

    /**
     * Gibt die höchste eigene Sequenznummer zurück (für die Summary-Zeile).
     */
    int maxLocalSeq() const;

    /**
     * Löscht Einträge älter als `daysOld` Tage aus sync_log.
     */
    void pruneLog(int daysOld = 30);

    QString deviceId() const { return m_deviceId; }
    QSqlDatabase sqlDb() const { return m_db; }

    /**
     * Speichert den Anzeigenamen eines Peers dauerhaft in sync_meta.
     * Wird bei jedem UDP-Kontakt aktualisiert.
     */
    void setPeerName(const QString &deviceId, const QString &name);

    /**
     * Gibt den gespeicherten Anzeigenamen zurück.
     * Fallback: erste 12 Zeichen der deviceId + "…"
     */
    QString peerDisplayName(const QString &deviceId) const;
    QString peerDisplayNameShort(const QString &deviceId) const;

signals:
    void changeApplied(const QString &table, int rowId);

private:
    bool createSyncTables();
    bool installTriggersForTable(const QString &tableName);
    bool migrateExistingData();
    bool migrateSchemaV2();  // Ergänzt local_seq-Spalte + sync_knowledge-Tabelle

    bool applyUpsert(const QString &table, int rowId, const QJsonObject &data);
    bool applyDelete(const QString &table, int rowId);

    // Knowledge Matrix: höchste local_seq die peer von origin_device schon hat
    void updateKnowledge(const QString &peerDeviceId,
                         const QString &originDeviceId,
                         int maxSeq);
    int  getKnownSeq(const QString &peerDeviceId,
                     const QString &originDeviceId) const;

    QStringList columnNames(const QString &tableName) const;
    QJsonObject rowToJson(const QString &tableName, int rowId) const;

    QSqlDatabase m_db;
    QString      m_deviceId;

    static const QStringList kSyncTables;
};
