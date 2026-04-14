#include "filetransfer.h"
#include <QJsonDocument>
#include <QDataStream>
#include <QDebug>

FileTransfer::FileTransfer(QObject *parent)
    : QObject(parent)
{
    connect(&m_server, &QTcpServer::newConnection,
            this,      &FileTransfer::onNewConnection);
}

FileTransfer::~FileTransfer()
{
    stopServer();
}

bool FileTransfer::startServer(quint16 port)
{
    if (!m_server.listen(QHostAddress::Any, port)) {
        qWarning() << "[FileTransfer] listen failed:" << m_server.errorString();
        return false;
    }
    qDebug() << "[FileTransfer] server listening on port" << port;
    return true;
}

void FileTransfer::stopServer()
{
    m_server.close();
    for (QTcpSocket *s : m_buffers.keys()) {
        s->disconnectFromHost();
        s->deleteLater();
    }
    m_buffers.clear();
}

bool FileTransfer::isListening() const
{
    return m_server.isListening();
}

// ── Server-Seite ──────────────────────────────────────────────────────────

void FileTransfer::onNewConnection()
{
    while (m_server.hasPendingConnections()) {
        QTcpSocket *socket = m_server.nextPendingConnection();
        m_buffers[socket] = QByteArray();

        connect(socket, &QTcpSocket::readyRead,
                this,   &FileTransfer::onReadyRead);
        connect(socket, &QTcpSocket::disconnected,
                this,   &FileTransfer::onSocketDisconnected);

        qDebug() << "[FileTransfer] neue Verbindung von" << socket->peerAddress().toString();
    }
}

void FileTransfer::onReadyRead()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    m_buffers[socket].append(socket->readAll());
    processBuffer(socket);
}

void FileTransfer::processBuffer(QTcpSocket *socket)
{
    QByteArray &buf = m_buffers[socket];

    // Solange vollständige Nachrichten im Puffer vorhanden sind:
    while (buf.size() >= 4) {
        // Länge lesen (4 Byte Little-Endian)
        quint32 msgLen = 0;
        msgLen |= static_cast<quint32>(static_cast<quint8>(buf[0]));
        msgLen |= static_cast<quint32>(static_cast<quint8>(buf[1])) << 8;
        msgLen |= static_cast<quint32>(static_cast<quint8>(buf[2])) << 16;
        msgLen |= static_cast<quint32>(static_cast<quint8>(buf[3])) << 24;

        if (static_cast<int>(4 + msgLen) > buf.size()) break; // noch nicht vollständig

        QByteArray payload = buf.mid(4, static_cast<int>(msgLen));
        buf.remove(0, static_cast<int>(4 + msgLen));

        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(payload, &err);
        if (err.error == QJsonParseError::NoError) {
            if (doc.isArray()) {
                qDebug() << "[FileTransfer] Empfangen:" << doc.array().size() << "Änderungen";
                emit changesReceived(doc.array());
            } else if (doc.isObject() && doc.object()["full_export"].toBool()) {
                qDebug() << "[FileTransfer] Vollstaendiger Snapshot empfangen";
                emit fullExportReceived(doc.object());
            } else if (doc.isObject() && doc.object()["full_export_request"].toBool()) {
                qDebug() << "[FileTransfer] FULL_EXPORT_REQUEST von"
                         << socket->peerAddress().toString();
                emit fullExportRequested(socket->peerAddress());
            } else if (doc.isObject() && doc.object()["sync_request"].toBool()) {
                const QString senderId = doc.object()["device_id"].toString();
                qDebug() << "[FileTransfer] SYNC_REQUEST von" << socket->peerAddress().toString()
                         << "device_id:" << senderId.left(8);
                emit syncRequested(socket->peerAddress(), senderId);
            } else {
                qWarning() << "[FileTransfer] Unbekanntes JSON-Format empfangen";
            }
        } else {
            qWarning() << "[FileTransfer] JSON-Fehler:" << err.errorString();
        }
    }
}

void FileTransfer::onSocketDisconnected()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;
    m_buffers.remove(socket);
    socket->deleteLater();
}

// ── Client-Seite ──────────────────────────────────────────────────────────

void FileTransfer::sendChanges(const QHostAddress &host, quint16 port,
                                const QJsonArray &changes,
                                std::function<void()> onSent)
{
    if (changes.isEmpty()) {
        qDebug() << "[FileTransfer] keine Änderungen zu senden";
        return;
    }

    QTcpSocket *sock = new QTcpSocket(this);

    connect(sock, &QTcpSocket::connected, this, [sock, changes]() {
        QJsonDocument doc(changes);
        QByteArray payload = doc.toJson(QJsonDocument::Compact);
        sock->write(FileTransfer::frameMessage(payload));
        sock->flush();
        qDebug() << "[FileTransfer] Gesendet:" << changes.size() << "Änderungen";
        sock->disconnectFromHost();
    });

    // onSent erst nach sauberem Disconnect aufrufen –
    // so startet syncNextPeer() erst wenn die Verbindung wirklich geschlossen ist.
    connect(sock, &QTcpSocket::disconnected, this, [sock, onSent]() {
        sock->deleteLater();
        if (onSent) onSent();
    });

    connect(sock, &QAbstractSocket::errorOccurred, this,
            [sock, onSent](QAbstractSocket::SocketError) {
        qWarning() << "[FileTransfer] Sendefehler:" << sock->errorString();
        sock->deleteLater();
        // onSent trotzdem aufrufen – sonst bleibt syncNextPeer() stecken
        // und markAsSent() wird nie erreicht (Windows: dauerhaft "ausstehend")
        if (onSent) onSent();
    });

    sock->connectToHost(host, port);
}

// ── Hilfsfunktionen ───────────────────────────────────────────────────────

void FileTransfer::requestChanges(const QHostAddress &host, quint16 port,
                                   const QString &senderDeviceId)
{
    QTcpSocket *sock = new QTcpSocket(this);

    connect(sock, &QTcpSocket::connected, this, [sock, senderDeviceId]() {
        QJsonObject req;
        req["sync_request"] = true;
        if (!senderDeviceId.isEmpty())
            req["device_id"] = senderDeviceId;
        QByteArray payload = QJsonDocument(req).toJson(QJsonDocument::Compact);
        sock->write(FileTransfer::frameMessage(payload));
        sock->flush();
        qDebug() << "[FileTransfer] SYNC_REQUEST gesendet an"
                 << sock->peerAddress().toString();
        sock->disconnectFromHost();
    });

    connect(sock, &QTcpSocket::disconnected, sock, &QTcpSocket::deleteLater);
    connect(sock, &QAbstractSocket::errorOccurred, this,
            [sock](QAbstractSocket::SocketError) {
        qWarning() << "[FileTransfer] requestChanges Fehler:" << sock->errorString();
        sock->deleteLater();
    });

    sock->connectToHost(host, port);
}

void FileTransfer::requestFullExport(const QHostAddress &host, quint16 port)
{
    // Neues Gerät (leere DB) bittet um vollständigen Datensatz.
    // Der Empfänger antwortet mit sendFullExport().
    QTcpSocket *sock = new QTcpSocket(this);

    connect(sock, &QTcpSocket::connected, this, [sock]() {
        QJsonObject req;
        req["full_export_request"] = true;
        QByteArray payload = QJsonDocument(req).toJson(QJsonDocument::Compact);
        sock->write(FileTransfer::frameMessage(payload));
        sock->flush();
        qDebug() << "[FileTransfer] FULL_EXPORT_REQUEST gesendet an"
                 << sock->peerAddress().toString();
        sock->disconnectFromHost();
    });

    connect(sock, &QTcpSocket::disconnected, sock, &QTcpSocket::deleteLater);
    connect(sock, &QAbstractSocket::errorOccurred, this,
            [sock](QAbstractSocket::SocketError) {
        qWarning() << "[FileTransfer] requestFullExport Fehler:" << sock->errorString();
        sock->deleteLater();
    });

    sock->connectToHost(host, port);
}

QByteArray FileTransfer::frameMessage(const QByteArray &payload)
{
    quint32 len = static_cast<quint32>(payload.size());
    QByteArray frame(4, '\0');
    frame[0] = static_cast<char>(len & 0xFF);
    frame[1] = static_cast<char>((len >> 8)  & 0xFF);
    frame[2] = static_cast<char>((len >> 16) & 0xFF);
    frame[3] = static_cast<char>((len >> 24) & 0xFF);
    frame.append(payload);
    return frame;
}

void FileTransfer::sendFullExport(const QHostAddress &host, quint16 port,
                                   const QJsonObject &snapshot,
                                   std::function<void()> onSent)
{
    QTcpSocket *sock = new QTcpSocket(this);

    connect(sock, &QTcpSocket::connected, this, [sock, snapshot, onSent]() {
        QByteArray payload = QJsonDocument(snapshot).toJson(QJsonDocument::Compact);
        sock->write(FileTransfer::frameMessage(payload));
        sock->flush();
        qDebug() << "[FileTransfer] Vollständiger Snapshot gesendet:"
                 << payload.size() << "Bytes";
        if (onSent) onSent();
        sock->disconnectFromHost();
    });

    connect(sock, &QTcpSocket::disconnected, sock, &QTcpSocket::deleteLater);
    connect(sock, &QAbstractSocket::errorOccurred, this,
            [sock](QAbstractSocket::SocketError) {
        qWarning() << "[FileTransfer] Sendefehler (fullExport):" << sock->errorString();
        sock->deleteLater();
    });

    sock->connectToHost(host, port);
}
