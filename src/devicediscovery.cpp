#include "devicediscovery.h"
#include <QNetworkInterface>
#include <QDebug>
#if defined(Q_OS_ANDROID)
#  include <QJniObject>
#endif

DeviceDiscovery::DeviceDiscovery(const QString &deviceId, QObject *parent)
    : QObject(parent)
    , m_deviceId(deviceId)
    , m_deviceName(buildDeviceName())
{
    connect(&m_heartbeatTimer, &QTimer::timeout,
            this,              &DeviceDiscovery::sendHeartbeat);
    connect(&m_udpSocket, &QUdpSocket::readyRead,
            this,         &DeviceDiscovery::readPendingDatagrams);

    m_autoStopTimer.setSingleShot(true);
    connect(&m_autoStopTimer, &QTimer::timeout, this, [this]() {
        qDebug() << "[DeviceDiscovery] Auto-Stop: keine Activity mehr – Discovery beendet";
        stop();
    });

    qDebug() << "[DeviceDiscovery] Gerätename:" << m_deviceName;
}

// ── Gerätename ────────────────────────────────────────────────────────────

QString DeviceDiscovery::buildDeviceName()
{
    QString name;

#if defined(Q_OS_ANDROID)
    // Android: Herstellermodell aus Build.MODEL
    name = QJniObject::getStaticObjectField<jstring>(
               "android/os/Build", "MODEL").toString();
    if (name.isEmpty())
        name = QStringLiteral("Android");
    else
        name += QStringLiteral(" (Android)");
#else
    // Windows / sonstige: Hostname + OS
    name = QSysInfo::machineHostName();
    const QString os = QSysInfo::productType();
    if (!os.isEmpty() && os != QStringLiteral("unknown"))
        name += QStringLiteral(" (") + os + QStringLiteral(")");
#endif

    // '|' ist unser Trennzeichen im Heartbeat – im Namen ersetzen
    name.replace('|', '-');
    return name;
}

// ── Android WLAN-Check ────────────────────────────────────────────────────

#if defined(Q_OS_ANDROID)
bool DeviceDiscovery::isWifiConnected()
{
    // ConnectivityManager.getNetworkCapabilities(activeNetwork).hasTransport(TRANSPORT_WIFI)
    // TRANSPORT_WIFI = 1 (NetworkCapabilities Konstante)
    QJniObject activity = QJniObject::callStaticObjectMethod(
        "org/qtproject/qt/android/QtNative", "activity",
        "()Landroid/app/Activity;");
    if (!activity.isValid()) return true;

    QJniObject connManager = activity.callObjectMethod(
        "getSystemService",
        "(Ljava/lang/String;)Ljava/lang/Object;",
        QJniObject::fromString("connectivity").object<jstring>());
    if (!connManager.isValid()) return true;

    QJniObject activeNetwork = connManager.callObjectMethod(
        "getActiveNetwork", "()Landroid/net/Network;");
    if (!activeNetwork.isValid()) {
        qDebug() << "[DeviceDiscovery] isWifiConnected: kein aktives Netzwerk";
        return false;
    }

    QJniObject caps = connManager.callObjectMethod(
        "getNetworkCapabilities",
        "(Landroid/net/Network;)Landroid/net/NetworkCapabilities;",
        activeNetwork.object());
    if (!caps.isValid()) return false;

    return caps.callMethod<jboolean>("hasTransport", "(I)Z", 1); // TRANSPORT_WIFI = 1
}

bool DeviceDiscovery::isVpnActive()
{
    // TRANSPORT_VPN = 4 – prüft ob das aktive Netzwerk über ein VPN getunnelt wird
    QJniObject activity = QJniObject::callStaticObjectMethod(
        "org/qtproject/qt/android/QtNative", "activity",
        "()Landroid/app/Activity;");
    if (!activity.isValid()) return false;

    QJniObject connManager = activity.callObjectMethod(
        "getSystemService",
        "(Ljava/lang/String;)Ljava/lang/Object;",
        QJniObject::fromString("connectivity").object<jstring>());
    if (!connManager.isValid()) return false;

    QJniObject activeNetwork = connManager.callObjectMethod(
        "getActiveNetwork", "()Landroid/net/Network;");
    if (!activeNetwork.isValid()) return false;

    QJniObject caps = connManager.callObjectMethod(
        "getNetworkCapabilities",
        "(Landroid/net/Network;)Landroid/net/NetworkCapabilities;",
        activeNetwork.object());
    if (!caps.isValid()) return false;

    return caps.callMethod<jboolean>("hasTransport", "(I)Z", 4); // TRANSPORT_VPN = 4
}
#endif

// ── Discovery ─────────────────────────────────────────────────────────────

bool DeviceDiscovery::start(quint16 port, int durationMs)
{
    if (m_running) {
        // Bereits laufend: Auto-Stop-Timer neu setzen falls durationMs angegeben
        if (durationMs > 0) {
            m_autoStopTimer.start(durationMs);
            qDebug() << "[DeviceDiscovery] bereits laufend – Auto-Stop-Timer neu:"
                     << durationMs << "ms";
        }
        return true;
    }
    m_port = port;

#if defined(Q_OS_ANDROID)
    // WLAN-Check: Broadcasts ohne WLAN sind sinnlos und wecken den Radio auf
    if (!isWifiConnected()) {
        qDebug() << "[DeviceDiscovery] start: kein WLAN aktiv – Discovery nicht gestartet";
        return false;
    }

    // MulticastLock anfordern – ohne diesen Lock filtert Android
    // eingehende UDP-Broadcasts/Multicast-Pakete still weg.
    // CHANGE_WIFI_MULTICAST_STATE-Permission muss im Manifest stehen.
    if (!m_multicastLock.isValid()) {
        QJniObject activity = QJniObject::callStaticObjectMethod(
            "org/qtproject/qt/android/QtNative", "activity",
            "()Landroid/app/Activity;");
        if (activity.isValid()) {
            QJniObject wifiServiceStr = QJniObject::fromString("wifi");
            QJniObject wifiManager = activity.callObjectMethod(
                "getSystemService",
                "(Ljava/lang/String;)Ljava/lang/Object;",
                wifiServiceStr.object<jstring>());
            if (wifiManager.isValid()) {
                QJniObject lockName = QJniObject::fromString("FahrtenbuchSyncLock");
                m_multicastLock = wifiManager.callObjectMethod(
                    "createMulticastLock",
                    "(Ljava/lang/String;)Landroid/net/wifi/WifiManager$MulticastLock;",
                    lockName.object<jstring>());
            }
        }
    }
    if (m_multicastLock.isValid()) {
        m_multicastLock.callMethod<void>("acquire");
        qDebug() << "[DeviceDiscovery] MulticastLock acquired";
    } else {
        qWarning() << "[DeviceDiscovery] MulticastLock konnte nicht erstellt werden";
    }
#endif

    if (!m_udpSocket.bind(m_port,
                          QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        qWarning() << "[DeviceDiscovery] bind FEHLER auf Port" << m_port
                   << "–" << m_udpSocket.errorString();
        return false;
    }

    m_running = true;
    m_heartbeatTimer.start(5000);
    sendHeartbeat();

    // Auto-Stop-Timer starten (On-demand-Modus)
    if (durationMs > 0) {
        m_autoStopTimer.start(durationMs);
        qDebug() << "[DeviceDiscovery] started on port" << m_port
                 << "| Auto-Stop in" << durationMs / 1000 << "s";
    } else {
        m_autoStopTimer.stop();
        qDebug() << "[DeviceDiscovery] started on port" << m_port << "(dauerhaft)";
    }

    return true;
}

void DeviceDiscovery::stop()
{
    m_autoStopTimer.stop();
    m_heartbeatTimer.stop();
    m_udpSocket.close();
    m_running = false;
    m_lastSeen.clear();
    m_everSeen.clear();
    m_replied.clear();
    m_ipToDeviceId.clear();

#if defined(Q_OS_ANDROID)
    if (m_multicastLock.isValid()) {
        // isHeld() prüfen bevor release() – sonst Exception
        jboolean held = m_multicastLock.callMethod<jboolean>("isHeld");
        if (held) {
            m_multicastLock.callMethod<void>("release");
            qDebug() << "[DeviceDiscovery] MulticastLock released";
        }
    }
#endif

    qDebug() << "[DeviceDiscovery] stopped";
}

void DeviceDiscovery::sendHeartbeat()
{
#if defined(Q_OS_ANDROID)
    // WLAN-Check bei jedem Heartbeat: falls WLAN weggefallen → Discovery stoppen
    if (!isWifiConnected()) {
        qDebug() << "[DeviceDiscovery] sendHeartbeat: WLAN nicht mehr aktiv – Discovery stoppt";
        stop();
        return;
    }
#endif

    QByteArray msg = QByteArrayLiteral("DEVICE_HERE:")
                     + m_deviceId.toUtf8()
                     + '|'
                     + m_deviceName.toUtf8();

    // Gezielte Subnetz-Broadcasts senden (funktioniert besser als 255.255.255.255
    // auf Android 13+ wo globaler Broadcast geblockt werden kann).
    bool sentAny = false;
    const auto ifaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &iface : ifaces) {
        if (!(iface.flags() & QNetworkInterface::IsUp)) continue;
        if (!(iface.flags() & QNetworkInterface::CanBroadcast)) continue;
        if (iface.flags() & QNetworkInterface::IsLoopBack) continue;
        for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
            if (entry.ip().protocol() != QAbstractSocket::IPv4Protocol) continue;
            const QHostAddress bcast = entry.broadcast();
            if (bcast.isNull()) continue;
            qint64 w = m_udpSocket.writeDatagram(msg, bcast, m_port);
            qDebug() << "[DeviceDiscovery] Heartbeat ->" << bcast.toString()
                     << "bytes:" << w
                     << (w < 0 ? m_udpSocket.errorString() : QString());
            sentAny = true;
        }
    }
    if (!sentAny) {
        qint64 w = m_udpSocket.writeDatagram(msg, QHostAddress::Broadcast, m_port);
        qDebug() << "[DeviceDiscovery] Heartbeat (fallback) -> 255.255.255.255 bytes:" << w;
    }
}

void DeviceDiscovery::readPendingDatagrams()
{
    while (m_udpSocket.hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(static_cast<int>(m_udpSocket.pendingDatagramSize()));
        QHostAddress sender;
        m_udpSocket.readDatagram(datagram.data(), datagram.size(), &sender);

        if (!datagram.startsWith("DEVICE_HERE:")) continue;

        qDebug() << "[DeviceDiscovery] Datagramm von" << sender.toString()
                 << ":" << datagram.left(60);

        // Format parsen: "<id>" oder "<id>|<n>"
        const QString raw      = QString::fromUtf8(datagram.mid(12));
        const int     pipeIdx  = raw.indexOf('|');
        const QString remoteId = (pipeIdx >= 0) ? raw.left(pipeIdx) : raw;
        const QString remoteName = (pipeIdx >= 0) ? raw.mid(pipeIdx + 1)
                                                   : raw.left(8);  // Fallback: ID-Prefix

        // Eigene Pakete ignorieren
        if (remoteId == m_deviceId) continue;

        // Unicast-Antwort beim ersten Kontakt
        if (!m_replied.contains(remoteId)) {
            m_replied.insert(remoteId);
            QByteArray reply = QByteArrayLiteral("DEVICE_HERE:")
                               + m_deviceId.toUtf8()
                               + '|'
                               + m_deviceName.toUtf8();
            m_udpSocket.writeDatagram(reply, sender, m_port);
            qDebug() << "[DeviceDiscovery] Erstkontakt – Unicast-Antwort an"
                     << sender.toString() << "(" << remoteName << ")";
        }

        m_ipToDeviceId[sender.toString()] = remoteId;

        // Cooldown
        const QString senderKey = sender.toString();
        const qint64  now       = QDateTime::currentMSecsSinceEpoch();
        const qint64  elapsed   = now - m_lastSeen.value(senderKey, 0);
        const qint64  cooldown  = m_everSeen.contains(senderKey)
                                  ? kCooldownLongMs : kCooldownMs;
        if (elapsed < cooldown) continue;

        m_lastSeen[senderKey] = now;
        m_everSeen.insert(senderKey);

        // Auto-Stop-Timer verlängern: frisches Gerät → Discovery noch kExtendMs weiter
        if (m_autoStopTimer.isActive()) {
            m_autoStopTimer.start(kExtendMs);
            qDebug() << "[DeviceDiscovery] Auto-Stop verlängert um"
                     << kExtendMs / 1000 << "s";
        }

        qDebug() << "[DeviceDiscovery] deviceFound:" << senderKey
                 << "id:" << remoteId.left(8) << "name:" << remoteName;
        emit deviceFound(sender, remoteId, remoteName);
    }
}
