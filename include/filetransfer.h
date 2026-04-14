#pragma once
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QHostAddress>
#include <QMap>
#include <functional>

/**
 * FileTransfer – TCP-basierter JSON-Austausch für Sync-Änderungen.
 *
 * Protokoll: [4 Byte Little-Endian Länge][JSON-Payload UTF-8]
 * Damit werden unvollständige TCP-Fragmente korrekt behandelt.
 *
 * Server: wartet passiv auf eingehende Verbindungen.
 * Client: sendChanges() baut Verbindung auf, sendet, trennt.
 */
class FileTransfer : public QObject
{
    Q_OBJECT
public:
    explicit FileTransfer(QObject *parent = nullptr);
    ~FileTransfer();

    bool startServer(quint16 port = 45454);
    void stopServer();
    bool isListening() const;

    // Asynchron – kehrt sofort zurück; Fehler werden geloggt
    /**
     * Sendet changes async an host:port.
     * onSent() wird aufgerufen sobald die Daten tatsächlich auf den Socket
     * geschrieben wurden (nach Verbindungsaufbau), NICHT sofort.
     */
    void sendChanges(const QHostAddress &host, quint16 port,
                     const QJsonArray &changes,
                     std::function<void()> onSent = nullptr);

    /** Sendet SYNC_REQUEST – bittet Gegenstück seine pending Änderungen zu senden. */
    void requestChanges(const QHostAddress &host, quint16 port, const QString &senderDeviceId = {});

    /** Sendet FULL_EXPORT_REQUEST – neues Gerät bittet um vollständigen Datensatz. */
    void requestFullExport(const QHostAddress &host, quint16 port);

    /** Sendet vollständigen DB-Snapshot für Erstabgleich. */
    void sendFullExport(const QHostAddress &host, quint16 port,
                        const QJsonObject &snapshot,
                        std::function<void()> onSent = nullptr);

signals:
    void changesReceived(QJsonArray changes);
    /** Empfangener vollständiger Snapshot (Erstabgleich). */
    void fullExportReceived(QJsonObject snapshot);
    /** Gegenstück hat keine eigenen Änderungen, bittet uns unsere zu senden. */
    void syncRequested(QHostAddress peerIp, QString peerDeviceId);
    /** Neues Gerät bittet um vollständigen Datensatz (leere DB). */
    void fullExportRequested(QHostAddress peerIp);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onSocketDisconnected();

private:
    void processBuffer(QTcpSocket *socket);

    QTcpServer               m_server;
    // Pro Socket eigener Empfangspuffer
    QMap<QTcpSocket*, QByteArray> m_buffers;

    static QByteArray  frameMessage(const QByteArray &payload);
};
