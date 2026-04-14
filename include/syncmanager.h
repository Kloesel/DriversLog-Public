#pragma once
#include <QObject>
#include <QSettings>
#include <QQueue>
#include <QSet>
#include <QHostAddress>
#include <QJsonArray>
#include "database.h"
#include "devicediscovery.h"
#include "filetransfer.h"
#include "databasesync.h"

/**
 * SyncManager – koordiniert den WLAN-Sync.
 *
 * Zweistufige Initialisierung:
 *   1. initDb()    – immer beim App-Start aufrufen.
 *   2. startSync() – nur wenn syncMode == "wifi".
 *
 * Knowledge Matrix + Sequenznummern (ab v1.4.10):
 *   exportChanges(peerDeviceId) liefert nur was der Peer noch nicht hat.
 *   markAsSent(peerDeviceId, changes) aktualisiert die Matrix nach dem Senden.
 *   So gehen auch bei längerer Offline-Phase oder Verbindungsabbruch keine
 *   Änderungen verloren – beim nächsten Kontakt wird exakt das Delta gesendet.
 *
 * Mehrere Geräte werden sequenziell abgearbeitet (Queue):
 *   - exportChanges() wird pro Peer einzeln aufgerufen (peer-spezifisches Delta).
 *   - markAsSent() wird direkt nach jedem Peer aufgerufen (nicht erst am Ende).
 *
 * Akku-Schonung (ab v1.4.22):
 *   - Discovery läuft on-demand: 60 s nach Start/Resume, verlängerbar.
 *   - Alle 5 Min kurze Re-Discovery (30 s) damit neu hinzukommende Geräte
 *     auch nach der initialen Phase noch erkannt werden.
 *   - Android: WLAN-Check vor jedem Heartbeat; kein Broadcast ohne WLAN.
 */
class SyncManager : public QObject
{
    Q_OBJECT
public:
    explicit SyncManager(Database *db, QObject *parent = nullptr);
    ~SyncManager();

    void initDb();
    void applySettings(const Einstellungen &e);
    void startSync();
    void stopSync();

    /**
     * pauseSync() – stoppt Discovery + Relay-Timer (App im Hintergrund).
     * TCP-Server bleibt aktiv: der Laptop kann weiterhin Änderungen senden.
     * resumeSync() – startet Discovery + Relay-Timer neu (App im Vordergrund).
     * Nur auf Android relevant (applicationStateChanged).
     */
    void pauseSync();
    void resumeSync();

    void closeDb();
    bool isRunning() const { return m_running; }

    DatabaseSync *dbSync() const { return m_dbSync; }

signals:
    void syncStarted();
    void syncFinished(bool success, const QString &message);
    void syncProgress(const QString &message);

private slots:
    void onDeviceFound(const QHostAddress &ip, const QString &deviceId, const QString &deviceName);
    void onSyncRequested(const QHostAddress &ip, const QString &peerDeviceId);
    void onFullExportRequested(const QHostAddress &ip);
    void onFullExportReceived(const QJsonObject &snapshot);
    void onChangesReceived(const QJsonArray &changes);

private:
    void syncNextPeer();

    /**
     * Startet den Relay-Check-Timer nur wenn ausstehende Relay-Einträge
     * für bekannte Peers vorhanden sind. Stoppt ihn andernfalls.
     * Immer aufrufen statt m_relayCheckTimer->start() direkt.
     */
    void scheduleRelayIfNeeded();

    /**
     * Startet Discovery on-demand für durationMs ms (Standard 60 s).
     * Android: wird nur gestartet wenn WLAN aktiv (DeviceDiscovery::isWifiConnected).
     * Sicher mehrfach aufrufbar: laufende Discovery verlängert nur den Auto-Stop-Timer.
     */
    void startDiscoveryOnDemand(int durationMs = 60000);

    qint64 loadLastSync() const;
    void   saveLastSync(qint64 ts);

    Database         *m_db;
    DatabaseSync     *m_dbSync    = nullptr;
    DeviceDiscovery  *m_discovery = nullptr;
    FileTransfer     *m_transfer  = nullptr;
    Einstellungen     m_settings;
    bool              m_running    = false;
    bool              m_syncing    = false;
    bool              m_syncPaused = false;  // App im Hintergrund: Discovery pausiert, TCP-Server läuft
    QTimer           *m_relayCheckTimer         = nullptr;
    QTimer           *m_periodicDiscoveryTimer  = nullptr; // Alle 5 Min kurze Re-Discovery (30 s)
    QString           m_deviceId;

    QQueue<QHostAddress>    m_peerQueue;       // Geräte warten auf sequenziellen Sync
    QHash<QString, QString> m_ipToDeviceId;    // IP-String → deviceId (für Knowledge Matrix)
    bool                    m_waitingForSyncResponse = false; // SYNC_REQUEST läuft, Timer aktiv
    QSet<QString>           m_currentlySendingTo;             // Peers an die wir gerade senden (Race-Guard)

    // Timing-Konstanten für Discovery on-demand
    static constexpr int kDiscoveryInitMs     = 60000; // 60 s beim Start/Resume
    static constexpr int kDiscoveryPeriodicMs = 30000; // 30 s bei periodischem Re-Discovery
    static constexpr int kDiscoveryIntervalMs = 2 * 60 * 1000; // alle 2 Min
};
