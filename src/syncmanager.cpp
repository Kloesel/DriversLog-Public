#include "syncmanager.h"
#include "devicediscovery.h"
#include <QSqlDatabase>
#include <QUuid>
#include <QSettings>
#include <QDateTime>
#include <QDebug>
#include <QTimer>

static const char kKeyDeviceId[] = "sync/deviceId";
static const char kKeyLastSync[] = "sync/lastSyncMs";

/**
 * Normalisiert IPv4-mapped IPv6-Adressen (::ffff:x.x.x.x) zu reinen IPv4-Strings.
 * Hintergrund: UDP-Sockets liefern oft 192.168.2.x, TCP-Sockets ::ffff:192.168.2.x.
 * Ohne Normalisierung schlägt der Lookup in m_ipToDeviceId fehl.
 */
static QString ipKey(const QHostAddress &addr)
{
    bool ok = false;
    const quint32 v4 = addr.toIPv4Address(&ok);
    if (ok) return QHostAddress(v4).toString();
    return addr.toString();
}

SyncManager::SyncManager(Database *db, QObject *parent)
    : QObject(parent)
    , m_db(db)
{
    QSettings s;
    if (!s.contains(kKeyDeviceId))
        s.setValue(kKeyDeviceId, QUuid::createUuid().toString(QUuid::WithoutBraces));
    m_deviceId = s.value(kKeyDeviceId).toString();
    qDebug() << "[SyncManager] device_id:" << m_deviceId;
}

SyncManager::~SyncManager()
{
    stopSync();
    closeDb();
}

void SyncManager::closeDb()
{
    if (m_dbSync) {
        delete m_dbSync;
        m_dbSync = nullptr;
    }
}

// ── Schritt 1: DB-Initialisierung – immer beim App-Start ─────────────────

void SyncManager::initDb()
{
    qDebug() << "[SyncManager] initDb() aufgerufen";
    QSqlDatabase sqlDb = m_db->sqlDatabase();
    qDebug() << "[SyncManager] DB offen:" << sqlDb.isOpen()
             << "| Verbindung:" << sqlDb.connectionName()
             << "| Tabellen:" << sqlDb.tables();

    if (!sqlDb.isOpen()) {
        qWarning() << "[SyncManager] initDb: DB nicht offen";
        return;
    }

    if (!m_dbSync) {
        qDebug() << "[SyncManager] erstelle DatabaseSync, device_id:" << m_deviceId;
        m_dbSync = new DatabaseSync(sqlDb, m_deviceId, this);
        connect(m_dbSync, &DatabaseSync::changeApplied,
                this, [this](const QString &table, int) {
            emit syncProgress(tr("Änderung empfangen: %1").arg(table));
        });
    }

    qDebug() << "[SyncManager] rufe initializeSync()...";
    if (!m_dbSync->initializeSync()) {
        qWarning() << "[SyncManager] initDb: initializeSync fehlgeschlagen";
        return;  // Sync nicht starten wenn DB-Init fehlschlug
    } else {
        qDebug() << "[SyncManager] initializeSync OK";
    }

    // Eigenen Gerätenamen speichern (für SyncLog-Anzeige auf anderen Geräten)
    m_dbSync->setPeerName(m_deviceId, DeviceDiscovery::buildDeviceName());
    m_dbSync->pruneLog(30);
}

// ── Schritt 2: WLAN-Sync starten ─────────────────────────────────────────

void SyncManager::applySettings(const Einstellungen &e)
{
    const bool modeChanged = (e.syncMode   != m_settings.syncMode);
    const bool portChanged = (e.wifiUdpPort != m_settings.wifiUdpPort ||
                              e.wifiTcpPort != m_settings.wifiTcpPort);
    const bool needRestart = m_running && (modeChanged || portChanged);

    m_settings = e;

    if (needRestart) {
        // Ports oder Modus geändert → Neustart nötig
        stopSync();
    }

    if (m_settings.syncMode == "wifi" && !m_running) {
        startSync();
    } else if (m_settings.syncMode != "wifi" && m_running) {
        stopSync();
    }
}

void SyncManager::startSync()
{
    if (m_running) return;
    if (m_settings.syncMode != "wifi") {
        qDebug() << "[SyncManager] Sync deaktiviert (syncMode ≠ wifi)";
        return;
    }
    if (!m_dbSync) {
        qWarning() << "[SyncManager] startSync: m_dbSync null, rufe initDb() nach";
        initDb();
        if (!m_dbSync) return;
    }

    // TCP-Server
    if (!m_transfer) {
        m_transfer = new FileTransfer(this);
        connect(m_transfer, &FileTransfer::changesReceived,
                this,       &SyncManager::onChangesReceived);
        connect(m_transfer, &FileTransfer::syncRequested,
                this,       &SyncManager::onSyncRequested);
        connect(m_transfer, &FileTransfer::fullExportRequested,
                this,       &SyncManager::onFullExportRequested);
        connect(m_transfer, &FileTransfer::fullExportReceived,
                this,       &SyncManager::onFullExportReceived);
    }
    if (!m_transfer->isListening()) {
        if (!m_transfer->startServer(static_cast<quint16>(m_settings.wifiTcpPort))) {
            qWarning() << "[SyncManager] TCP-Server Start fehlgeschlagen, Port:" << m_settings.wifiTcpPort;
            emit syncFinished(false, tr("TCP-Server konnte nicht gestartet werden"));
            return;
        }
    }

    // UDP-Discovery (DeviceDiscovery-Objekt einmalig erstellen)
    if (!m_discovery) {
        m_discovery = new DeviceDiscovery(m_deviceId, this);
        connect(m_discovery, &DeviceDiscovery::deviceFound,
                this,        &SyncManager::onDeviceFound);
    }

    m_running = true;
    emit syncStarted();
    qDebug() << "[SyncManager] WLAN-Sync gestartet"
             << "UDP:" << m_settings.wifiUdpPort
             << "TCP:" << m_settings.wifiTcpPort;

    // Relay-Check-Timer
    if (!m_relayCheckTimer) {
        m_relayCheckTimer = new QTimer(this);
        m_relayCheckTimer->setInterval(30000);
        connect(m_relayCheckTimer, &QTimer::timeout, this, [this]() {
            if (!m_dbSync || m_syncing) return;
            for (auto it = m_ipToDeviceId.constBegin(); it != m_ipToDeviceId.constEnd(); ++it) {
                const QString &devId = it.value();
                if (devId.isEmpty()) continue;
                if (!m_dbSync->exportChanges(devId).isEmpty()) {
                    QHostAddress addr(it.key());
                    if (!m_peerQueue.contains(addr) && !m_currentlySendingTo.contains(devId)) {
                        qDebug() << "[SyncManager] RelayCheck: re-queue"
                                 << addr.toString() << "(" << devId.left(8) << ")";
                        m_peerQueue.enqueue(addr);
                    }
                }
            }
            if (!m_peerQueue.isEmpty() && !m_syncing)
                syncNextPeer();

            scheduleRelayIfNeeded();
        });
    }
    scheduleRelayIfNeeded();

    // Periodischer Re-Discovery-Timer: alle 5 Min kurze 30-s-Session starten
    // damit Geräte die später ins WLAN kommen noch erkannt werden.
    if (!m_periodicDiscoveryTimer) {
        m_periodicDiscoveryTimer = new QTimer(this);
        m_periodicDiscoveryTimer->setInterval(kDiscoveryIntervalMs);
        connect(m_periodicDiscoveryTimer, &QTimer::timeout, this, [this]() {
            if (m_syncPaused) return;
            qDebug() << "[SyncManager] Periodischer Re-Discovery-Trigger";
            startDiscoveryOnDemand(kDiscoveryPeriodicMs);
        });
    }
    m_periodicDiscoveryTimer->start();

    // Initiale Discovery-Session (60 s)
    startDiscoveryOnDemand(kDiscoveryInitMs);
}

// ── Discovery on-demand ───────────────────────────────────────────────────

void SyncManager::startDiscoveryOnDemand(int durationMs)
{
    if (!m_running || m_syncPaused || !m_discovery) return;

    const bool started = m_discovery->start(
        static_cast<quint16>(m_settings.wifiUdpPort), durationMs);

    if (started) {
        qDebug() << "[SyncManager] Discovery on-demand gestartet für"
                 << durationMs / 1000 << "s";
    } else {
        qDebug() << "[SyncManager] Discovery on-demand: kein WLAN – übersprungen";
    }
}

void SyncManager::scheduleRelayIfNeeded()
{
    if (!m_relayCheckTimer || !m_running || m_syncPaused) {
        if (m_relayCheckTimer) m_relayCheckTimer->stop();
        return;
    }

    bool hasPending = false;
    if (m_dbSync) {
        for (auto it = m_ipToDeviceId.constBegin(); it != m_ipToDeviceId.constEnd(); ++it) {
            const QString &devId = it.value();
            if (!devId.isEmpty() && !m_dbSync->exportChanges(devId).isEmpty()) {
                hasPending = true;
                break;
            }
        }
    }

    if (hasPending) {
        if (!m_relayCheckTimer->isActive()) {
            qDebug() << "[SyncManager] scheduleRelayIfNeeded: ausstehende Relays → Timer gestartet";
            m_relayCheckTimer->start();
        }
    } else {
        if (m_relayCheckTimer->isActive()) {
            qDebug() << "[SyncManager] scheduleRelayIfNeeded: keine Relays → Timer gestoppt";
            m_relayCheckTimer->stop();
        }
    }
}

// ── Akku-Schonung: Pause/Resume (Android – App im Hintergrund) ───────────

void SyncManager::pauseSync()
{
    if (!m_running || m_syncPaused) return;
    m_syncPaused = true;

    if (m_discovery)              m_discovery->stop();
    if (m_relayCheckTimer)        m_relayCheckTimer->stop();
    if (m_periodicDiscoveryTimer) m_periodicDiscoveryTimer->stop();
    // TCP-Server läuft weiter – der Laptop kann weiterhin Änderungen senden

    qDebug() << "[SyncManager] pauseSync: Discovery + Relay-Timer + Periodic-Timer angehalten, TCP-Server aktiv";
}

void SyncManager::resumeSync()
{
    if (!m_running || !m_syncPaused) return;
    m_syncPaused = false;

    scheduleRelayIfNeeded();

    // Periodischen Re-Discovery-Timer wieder starten
    if (m_periodicDiscoveryTimer)
        m_periodicDiscoveryTimer->start();

    // Sofortige Discovery-Session (60 s) nach Vordergrund-Return
    startDiscoveryOnDemand(kDiscoveryInitMs);

    qDebug() << "[SyncManager] resumeSync: Discovery + Periodic-Timer neu gestartet";
}

void SyncManager::stopSync()
{
    if (!m_running) return;
    if (m_discovery)              m_discovery->stop();
    if (m_transfer)               m_transfer->stopServer();
    if (m_relayCheckTimer)        m_relayCheckTimer->stop();
    if (m_periodicDiscoveryTimer) m_periodicDiscoveryTimer->stop();
    m_peerQueue.clear();
    m_ipToDeviceId.clear();
    m_currentlySendingTo.clear();
    m_waitingForSyncResponse = false;
    m_syncing = false;
    m_running = false;
    qDebug() << "[SyncManager] WLAN-Sync gestoppt";
}

// ── Slots ─────────────────────────────────────────────────────────────────

void SyncManager::onDeviceFound(const QHostAddress &ip, const QString &deviceId,
                                const QString &deviceName)
{
    if (!m_dbSync) return;

    // IP → deviceId normalisiert speichern (beide Formen: 192.x und ::ffff:192.x)
    m_ipToDeviceId[ipKey(ip)] = deviceId;

    // Gerätenamen dauerhaft in sync_meta speichern (wird im SyncLog angezeigt)
    if (!deviceName.isEmpty())
        m_dbSync->setPeerName(deviceId, deviceName);

    // Doppelte IPs vermeiden (UDP-Broadcast kann mehrfach ankommen)
    if (m_peerQueue.contains(ip)) return;

    m_peerQueue.enqueue(ip);
    qDebug() << "[SyncManager] Gerät gefunden:" << ip.toString()
             << "| deviceId:" << deviceId.left(8)
             << "| Queue:" << m_peerQueue.size()
             << "| aktiver Sync:" << m_syncing;

    if (!m_syncing)
        syncNextPeer();
}

// Nächsten Peer aus der Queue sequenziell abarbeiten
void SyncManager::syncNextPeer()
{
    if (m_peerQueue.isEmpty()) {
        m_syncing = false;
        qDebug() << "[SyncManager] Alle Geräte synchronisiert";
        return;
    }

    m_syncing = true;
    QHostAddress ip         = m_peerQueue.dequeue();
    QString      peerDevId  = m_ipToDeviceId.value(ipKey(ip));

    qDebug() << "[SyncManager] Starte Sync mit" << ip.toString()
             << "| peerDevId:" << (peerDevId.isEmpty() ? "(unbekannt)" : peerDevId.left(8))
             << "| noch in Queue:" << m_peerQueue.size();

    // Knowledge-Matrix: nur was dieser Peer noch nicht hat
    QJsonArray pendingForPeer = m_dbSync->exportChanges(peerDevId);
    qDebug() << "[DatabaseSync] exportChanges für diesen Peer:" << pendingForPeer.size();

    if (pendingForPeer.isEmpty()) {
        // Neues Gerät? → Vollständigen Export anfordern statt Delta
        if (m_dbSync->isNewDevice()) {
            qDebug() << "[SyncManager] Leere DB erkannt – sende FULL_EXPORT_REQUEST an"
                     << ip.toString();
            emit syncProgress(tr("Fordere vollständigen Datensatz von %1 an …")
                              .arg(ip.toString()));
            m_transfer->requestFullExport(ip,
                                          static_cast<quint16>(m_settings.wifiTcpPort));
            // Timeout: falls Peer nicht antwortet (z.B. ebenfalls leere DB)
            QTimer::singleShot(10000, this, [this]() {
                if (m_syncing && m_waitingForSyncResponse) {
                    m_waitingForSyncResponse = false;
                    qDebug() << "[SyncManager] Timeout nach FULL_EXPORT_REQUEST – weiter";
                    syncNextPeer();
                }
            });
            m_waitingForSyncResponse = true;
            return;
        }

        // Keine Änderungen für diesen Peer → Gegenstück bitten seine zu senden
        qDebug() << "[SyncManager] Nichts zu senden – sende SYNC_REQUEST an"
                 << ip.toString();
        m_waitingForSyncResponse = true;
        m_transfer->requestChanges(ip,
                                   static_cast<quint16>(m_settings.wifiTcpPort),
                                   m_deviceId);
        // Timeout-Fallback: falls der Peer auch nichts hat (und daher nicht
        // zurückverbindet), würde die Queue sonst für immer blockieren.
        QTimer::singleShot(5000, this, [this]() {
            if (m_syncing && m_waitingForSyncResponse) {
                m_waitingForSyncResponse = false;
                qDebug() << "[SyncManager] Timeout nach SYNC_REQUEST – weiter";
                syncNextPeer();
            }
        });
        return;
    }

    emit syncProgress(tr("Sende %1 Änderungen an %2 …")
                      .arg(pendingForPeer.size()).arg(ip.toString()));

    DatabaseSync *dbSync = m_dbSync;
    m_currentlySendingTo.insert(peerDevId);
    m_transfer->sendChanges(ip,
                            static_cast<quint16>(m_settings.wifiTcpPort),
                            pendingForPeer,
                            [this, dbSync, pendingForPeer, peerDevId]() {
        m_currentlySendingTo.remove(peerDevId);
        dbSync->markAsSent(peerDevId, pendingForPeer);
        syncNextPeer();
    });
}

void SyncManager::onSyncRequested(const QHostAddress &ip, const QString &peerDeviceId)
{
    if (!m_dbSync) return;

    if (!peerDeviceId.isEmpty()) {
        const QString key = ipKey(ip);
        if (!m_ipToDeviceId.contains(key) || m_ipToDeviceId[key] != peerDeviceId) {
            m_ipToDeviceId[key] = peerDeviceId;
            qDebug() << "[SyncManager] IP→DeviceId registriert via SYNC_REQUEST:"
                     << ip.toString() << "->" << peerDeviceId.left(8);
        }
    }

    const QString peerDevId = m_ipToDeviceId.value(ipKey(ip));

    if (!peerDevId.isEmpty() && m_currentlySendingTo.contains(peerDevId)) {
        qDebug() << "[SyncManager] SYNC_REQUEST von" << ip.toString()
                 << "– ignoriert, sende bereits an" << peerDevId.left(8);
        return;
    }

    QJsonArray changes = m_dbSync->exportChanges(peerDevId);
    if (changes.isEmpty()) {
        qDebug() << "[SyncManager] SYNC_REQUEST von" << ip.toString()
                 << "– keine Änderungen für diesen Peer nötig";
        syncNextPeer();
        return;
    }

    emit syncProgress(tr("Sende %1 Änderungen an %2 …")
                      .arg(changes.size()).arg(ip.toString()));

    DatabaseSync *dbSync = m_dbSync;
    m_transfer->sendChanges(ip,
                            static_cast<quint16>(m_settings.wifiTcpPort),
                            changes,
                            [this, dbSync, changes, peerDevId]() {
        dbSync->markAsSent(peerDevId, changes);
        syncNextPeer();
    });
}

void SyncManager::onFullExportRequested(const QHostAddress &ip)
{
    if (!m_dbSync) return;

    qDebug() << "[SyncManager] FULL_EXPORT_REQUEST von" << ip.toString();

    if (m_dbSync->isNewDevice()) {
        qDebug() << "[SyncManager] Wir haben selbst keine Daten – ignoriere FULL_EXPORT_REQUEST";
        return;
    }

    emit syncProgress(tr("Neues Gerät erkannt – sende vollständigen Datensatz an %1 …")
                      .arg(ip.toString()));

    const QJsonObject snapshot = m_dbSync->fullExport();
    DatabaseSync *dbSync = m_dbSync;
    const QString peerDevId = m_ipToDeviceId.value(ipKey(ip));

    m_transfer->sendFullExport(ip,
                               static_cast<quint16>(m_settings.wifiTcpPort),
                               snapshot,
                               [dbSync, peerDevId, snapshot]() {
        qDebug() << "[SyncManager] fullExport gesendet";
        Q_UNUSED(dbSync); Q_UNUSED(peerDevId); Q_UNUSED(snapshot);
    });
}

void SyncManager::onFullExportReceived(const QJsonObject &snapshot)
{
    if (!m_dbSync) return;

    m_waitingForSyncResponse = false;

    emit syncProgress(tr("Vollständiger Datensatz empfangen – wird übernommen …"));

    const bool ok = m_dbSync->applyFullExport(snapshot);

    if (ok) {
        emit syncFinished(true, tr("Erstabgleich abgeschlossen – Daten vollständig übernommen"));
        qDebug() << "[SyncManager] applyFullExport OK";
    } else {
        emit syncFinished(false, tr("Fehler beim Erstabgleich"));
        qWarning() << "[SyncManager] applyFullExport FEHLER";
    }

    syncNextPeer();
}

void SyncManager::onChangesReceived(const QJsonArray &changes)
{
    if (!m_dbSync) return;

    m_waitingForSyncResponse = false;

    emit syncProgress(tr("%1 Änderungen empfangen …").arg(changes.size()));

    bool ok = m_dbSync->applyChanges(changes);

    if (ok) {
        m_dbSync->relayChanges(changes);

        for (auto it = m_ipToDeviceId.constBegin(); it != m_ipToDeviceId.constEnd(); ++it) {
            const QString &devId = it.value();
            if (devId.isEmpty()) continue;
            if (!m_dbSync->exportChanges(devId).isEmpty()) {
                QHostAddress addr(it.key());
                if (!m_peerQueue.contains(addr) && !m_currentlySendingTo.contains(devId)) {
                    qDebug() << "[SyncManager] Relay ausstehend – re-queue"
                             << addr.toString() << "(" << devId.left(8) << ")";
                    m_peerQueue.enqueue(addr);
                }
            }
        }

        scheduleRelayIfNeeded();
    }

    emit syncFinished(ok,
        ok ? tr("%1 Änderungen synchronisiert").arg(changes.size())
           : tr("Fehler beim Anwenden der Änderungen"));

    syncNextPeer();
}

// ── Persistenz ────────────────────────────────────────────────────────────

qint64 SyncManager::loadLastSync() const
{
    return QSettings().value(kKeyLastSync, qint64(0)).toLongLong();
}

void SyncManager::saveLastSync(qint64 ts)
{
    QSettings().setValue(kKeyLastSync, ts);
}
