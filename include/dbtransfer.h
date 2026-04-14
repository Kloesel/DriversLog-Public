#pragma once
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>

/**
 * DbTransfer – überträgt die komplette DB-Datei einmalig per TCP.
 *
 * Protokoll:
 *   [4 Byte Magic "FBDB"] [8 Byte Dateilänge LE] [Dateiinhalt]
 *
 * Sender (Android): connectToHost → sendet Magic + Länge + Dateiinhalt
 * Empfänger (Windows): wartet auf Port, prüft Magic, speichert Datei
 *
 * Port: wifiTcpPort + 1  (Standard: 45455)
 */
class DbTransfer : public QObject
{
    Q_OBJECT
public:
    explicit DbTransfer(QObject *parent = nullptr);
    ~DbTransfer();

    /** Windows: Empfänger starten. Gibt false zurück wenn Port belegt. */
    bool startReceiver(quint16 port);
    void stopReceiver();
    bool isReceiving() const { return m_server.isListening(); }

    /** Android: DB-Datei asynchron an host:port senden. */
    void sendDb(const QHostAddress &host, quint16 port, const QString &dbPath);

    /** IP-Adresse des eigenen Geräts im lokalen Netz (für Anzeige). */
    static QString localIpAddress();

signals:
    void sendProgress(int percent);
    void sendFinished(bool ok, const QString &message);

    void receiveProgress(int percent);
    /** Wird emittiert wenn eine komplette DB empfangen wurde.
     *  tmpPath: temporärer Pfad – Aufrufer muss die Datei verarbeiten. */
    void receiveFinished(bool ok, const QString &tmpPath, const QString &message);

private slots:
    void onNewConnection();
    void onServerReadyRead();
    void onServerDisconnected();

private:
    static const QByteArray kMagic;   // "FBDB"

    QTcpServer  m_server;

    // Pro Verbindung: Empfangspuffer + erwartete Dateigröße
    struct RecvState {
        QByteArray buf;
        qint64     expectedSize = -1;   // -1 = Header noch nicht gelesen
        QString    tmpPath;
    };
    QMap<QTcpSocket*, RecvState> m_recvStates;
};
