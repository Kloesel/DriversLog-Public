#pragma once
#include <QObject>
#include <QUdpSocket>
#include <QTimer>
#include <QHostAddress>
#include <QHash>
#include <QSet>
#include <QDateTime>
#include <QSysInfo>
#if defined(Q_OS_ANDROID)
#  include <QJniObject>
#endif

/**
 * DeviceDiscovery – UDP-Broadcast für automatische WLAN-Geräteerkennung.
 *
 * Heartbeat-Format: "DEVICE_HERE:<deviceId>|<deviceName>"
 * <deviceName> = QSysInfo::machineHostName() + " (" + Plattform + ")"
 * Rückwärtskompatibel: altes Format ohne "|" wird auch akzeptiert.
 *
 * Signal deviceFound(ip, deviceId, deviceName) wird emittiert wenn ein
 * fremdes Gerät erkannt wird (max. 1x pro IP innerhalb des Cooldown-Intervalls).
 *
 * On-demand-Modus (durationMs > 0):
 *   Discovery stoppt automatisch nach durationMs ms ohne neues Gerät.
 *   Wird ein Gerät gefunden, verlängert sich der Auto-Stop-Timer um kExtendMs.
 *
 * Android WLAN-Check:
 *   sendHeartbeat() prüft via ConnectivityManager ob WLAN verbunden ist.
 *   Ist kein WLAN aktiv, werden keine Broadcasts gesendet (Akku-Schonung).
 *   Fällt WLAN während einer Session weg, stoppt Discovery automatisch.
 */
class DeviceDiscovery : public QObject
{
    Q_OBJECT
public:
    explicit DeviceDiscovery(const QString &deviceId, QObject *parent = nullptr);

    /**
     * Startet Discovery.
     * @param port       UDP-Port (Standard 45455)
     * @param durationMs Auto-Stop nach durationMs ms (0 = dauerhaft, z.B. Windows)
     * @return true wenn gestartet, false wenn WLAN nicht verfügbar (Android)
     */
    bool start(quint16 port = 45455, int durationMs = 0);
    void stop();
    bool isRunning() const { return m_running; }

    /** Gerätename dieses Geräts (wie er im Netz angekündigt wird). */
    static QString buildDeviceName();

#if defined(Q_OS_ANDROID)
    /**
     * Prüft via ConnectivityManager ob WLAN-Transport aktiv ist.
     * Gibt im Fehlerfall (kein JNI-Kontext) true zurück, um Discovery nicht zu blockieren.
     */
    static bool isWifiConnected();
    static bool isVpnActive();
#endif

signals:
    void deviceFound(QHostAddress ip, QString deviceId, QString deviceName);

private slots:
    void sendHeartbeat();
    void readPendingDatagrams();

private:
    QString     m_deviceId;
    QString     m_deviceName;     // eigener Gerätename (im Heartbeat)
    QUdpSocket  m_udpSocket;
    QTimer      m_heartbeatTimer;
    QTimer      m_autoStopTimer;  // Auto-Stop im On-demand-Modus
    quint16     m_port    = 45455;
    bool        m_running = false;

    QHash<QString, qint64>  m_lastSeen;     // IP-String → letzter deviceFound-Zeitpunkt (ms)
    QSet<QString>           m_replied;      // remoteIds denen bereits geantwortet wurde
    QSet<QString>           m_everSeen;     // IPs die schon mindestens einmal emittiert wurden
    QHash<QString, QString> m_ipToDeviceId; // IP → deviceId

#if defined(Q_OS_ANDROID)
    QJniObject m_multicastLock; // WifiManager.MulticastLock – hält UDP-Broadcasts offen
#endif

    static constexpr qint64 kCooldownMs     =  8000;
    static constexpr qint64 kCooldownLongMs = 60000;

    /** Auto-Stop-Timer verlängern um diesen Wert wenn Gerät gefunden. */
    static constexpr int kExtendMs = 30000;
};
