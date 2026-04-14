#include "dbtransfer.h"
#include <QFile>
#include <QNetworkInterface>
#include <QStandardPaths>
#include <QDateTime>
#include <QDebug>

const QByteArray DbTransfer::kMagic = QByteArrayLiteral("FBDB");

DbTransfer::DbTransfer(QObject *parent)
    : QObject(parent)
{
    connect(&m_server, &QTcpServer::newConnection,
            this,      &DbTransfer::onNewConnection);
}

DbTransfer::~DbTransfer()
{
    stopReceiver();
}

// ── Empfänger (Windows) ───────────────────────────────────────────────────

bool DbTransfer::startReceiver(quint16 port)
{
    if (m_server.isListening()) return true;
    if (!m_server.listen(QHostAddress::Any, port)) {
        qWarning() << "[DbTransfer] listen failed:" << m_server.errorString();
        return false;
    }
    qDebug() << "[DbTransfer] Empfänger gestartet auf Port" << port;
    return true;
}

void DbTransfer::stopReceiver()
{
    m_server.close();
    for (QTcpSocket *s : m_recvStates.keys()) {
        s->disconnectFromHost();
        s->deleteLater();
    }
    m_recvStates.clear();
}

void DbTransfer::onNewConnection()
{
    while (m_server.hasPendingConnections()) {
        QTcpSocket *sock = m_server.nextPendingConnection();
        m_recvStates[sock] = RecvState{};

        connect(sock, &QTcpSocket::readyRead,
                this, &DbTransfer::onServerReadyRead);
        connect(sock, &QTcpSocket::disconnected,
                this, &DbTransfer::onServerDisconnected);

        qDebug() << "[DbTransfer] Verbindung von" << sock->peerAddress().toString();
    }
}

void DbTransfer::onServerReadyRead()
{
    QTcpSocket *sock = qobject_cast<QTcpSocket*>(sender());
    if (!sock) return;

    RecvState &st = m_recvStates[sock];
    st.buf.append(sock->readAll());

    // ── Header lesen: 4 Byte Magic + 8 Byte Länge LE ─────────────────────
    if (st.expectedSize < 0) {
        const int headerSize = 4 + 8;   // Magic + uint64
        if (st.buf.size() < headerSize) return;   // noch nicht genug

        if (!st.buf.startsWith(kMagic)) {
            qWarning() << "[DbTransfer] Ungültiges Magic – keine DB-Übertragung";
            sock->disconnectFromHost();
            return;
        }

        // Länge aus Bytes 4..11 (Little-Endian)
        quint64 fileSize = 0;
        for (int i = 0; i < 8; ++i)
            fileSize |= (quint64)(quint8)st.buf[4 + i] << (i * 8);

        st.expectedSize = static_cast<qint64>(fileSize);

        // Temporäre Datei anlegen
        const QString tmpDir = QStandardPaths::writableLocation(
            QStandardPaths::TempLocation);
        st.tmpPath = tmpDir + "/fahrtenbuch_recv.db";

        // Header aus Puffer entfernen
        st.buf.remove(0, headerSize);

        qDebug() << "[DbTransfer] Empfange DB," << st.expectedSize << "Bytes";
    }

    // ── Fortschritt melden ────────────────────────────────────────────────
    if (st.expectedSize > 0) {
        int pct = (int)((qint64)st.buf.size() * 100 / st.expectedSize);
        emit receiveProgress(qMin(pct, 99));
    }

    // ── Vollständig empfangen? ────────────────────────────────────────────
    if ((qint64)st.buf.size() >= st.expectedSize) {
        QFile f(st.tmpPath);
        if (f.open(QIODevice::WriteOnly)) {
            f.write(st.buf.left((int)st.expectedSize));
            f.close();
            qDebug() << "[DbTransfer] DB empfangen:" << st.tmpPath;
            emit receiveProgress(100);
            emit receiveFinished(true, st.tmpPath, tr("Datenbank empfangen"));
        } else {
            emit receiveFinished(false, {}, tr("Konnte Datei nicht schreiben: %1")
                                 .arg(st.tmpPath));
        }
        sock->disconnectFromHost();
    }
}

void DbTransfer::onServerDisconnected()
{
    QTcpSocket *sock = qobject_cast<QTcpSocket*>(sender());
    if (!sock) return;
    m_recvStates.remove(sock);
    sock->deleteLater();
}

// ── Sender (Android) ──────────────────────────────────────────────────────

void DbTransfer::sendDb(const QHostAddress &host, quint16 port,
                         const QString &dbPath)
{
    QFile f(dbPath);
    if (!f.open(QIODevice::ReadOnly)) {
        emit sendFinished(false, tr("Datenbank nicht lesbar: %1").arg(dbPath));
        return;
    }
    const QByteArray data = f.readAll();
    f.close();

    if (data.isEmpty()) {
        emit sendFinished(false, tr("Datenbank ist leer"));
        return;
    }

    // Frame aufbauen: Magic(4) + Länge(8 LE) + Daten
    QByteArray frame;
    frame.append(kMagic);
    quint64 len = static_cast<quint64>(data.size());
    for (int i = 0; i < 8; ++i)
        frame.append(static_cast<char>((len >> (i * 8)) & 0xFF));
    frame.append(data);

    QTcpSocket *sock = new QTcpSocket(this);

    connect(sock, &QTcpSocket::connected, this, [this, sock, frame]() {
        qDebug() << "[DbTransfer] Verbunden, sende" << frame.size() << "Bytes";
        sock->write(frame);
        sock->flush();
        emit sendProgress(100);
        sock->disconnectFromHost();
        emit sendFinished(true, tr("Datenbank gesendet"));
    });

    connect(sock, &QTcpSocket::disconnected, sock, &QTcpSocket::deleteLater);

    connect(sock, &QAbstractSocket::errorOccurred, this,
            [this, sock](QAbstractSocket::SocketError) {
        const QString err = sock->errorString();
        qWarning() << "[DbTransfer] Sendefehler:" << err;
        sock->deleteLater();
        emit sendFinished(false, tr("Verbindungsfehler: %1").arg(err));
    });

    emit sendProgress(0);
    sock->connectToHost(host, port);
    qDebug() << "[DbTransfer] Verbinde mit" << host.toString() << ":" << port;
}

// ── Hilfsfunktion ─────────────────────────────────────────────────────────

QString DbTransfer::localIpAddress()
{
    for (const QNetworkInterface &iface : QNetworkInterface::allInterfaces()) {
        if (iface.flags() & QNetworkInterface::IsLoopBack) continue;
        if (!(iface.flags() & QNetworkInterface::IsUp))    continue;
        if (!(iface.flags() & QNetworkInterface::IsRunning)) continue;

        for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
            const QHostAddress addr = entry.ip();
            if (addr.protocol() == QAbstractSocket::IPv4Protocol) {
                const QString s = addr.toString();
                // Nur private Adressen (192.168.x.x / 10.x.x.x / 172.16-31.x.x)
                if (s.startsWith("192.168.") ||
                    s.startsWith("10.")      ||
                    s.startsWith("172."))
                    return s;
            }
        }
    }
    return tr("unbekannt");
}
