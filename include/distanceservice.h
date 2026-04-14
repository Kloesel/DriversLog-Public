#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QMap>
#include "models.h"

class DistanceService : public QObject
{
    Q_OBJECT
public:
    explicit DistanceService(QObject *parent = nullptr);

    void setApiKey(const QString &key) {
        // Schutz gegen versehentlich gespeicherte Dateipfade als API-Key.
        // Laut LogCat wurde einmalig api_key=file:///C:/Users/.../Update.zip
        // übergeben — das blockiert alle Routing-Anfragen mit HTTP-403.
        QString k = key.trimmed();
        bool looksLikePath = k.startsWith("file://", Qt::CaseInsensitive)
                          || k.startsWith("http://",  Qt::CaseInsensitive)
                          || k.startsWith("https://", Qt::CaseInsensitive)
                          || k.contains('\\')
                          || k.contains(".zip",  Qt::CaseInsensitive)
                          || k.contains(".exe",  Qt::CaseInsensitive)
                          || k.contains(".db",   Qt::CaseInsensitive)
                          || (k.length() > 3 && k[1] == ':'); // Windows-Laufwerk C:\...
        if (looksLikePath) {
            qWarning() << "DistanceService: Ungültiger API-Key ignoriert (sieht wie Dateipfad aus):" << k;
            m_apiKey.clear();
            return;
        }
        m_apiKey = k;
    }

    // Async: emits distanceReady when done
    void requestDistance(int requestId, const Adresse &from, const Adresse &to);

    // Fallback: straight-line (Haversine) distance in km
    static double haversine(double lat1, double lon1, double lat2, double lon2);

signals:
    void distanceReady(int requestId, double km, bool ok);
    void apiKeyInvalid();   // ORS 403 – Key ungueltig oder Tageslimit erreicht
    // Emittiert wenn eine Adresse (id>0) geocodiert wurde → Koordinaten in DB speichern
    void coordsResolved(int adresseId, double lat, double lon);

private slots:
    void onReplyFinished(QNetworkReply *reply);

private:
    // Ablauf: Geocoding (Nominatim, kostenlos) → Routing (ORS wenn Key, sonst OSRM) oder Haversine
    enum Stage { NominatimFrom, NominatimTo, OrsRouting, OsrmRouting };

    struct PendingRequest {
        int     requestId;
        Stage   stage;
        Adresse from;
        Adresse to;
        double  fromLat = 0.0, fromLon = 0.0;
        double  toLat   = 0.0, toLon   = 0.0;
    };

    void startNominatimFrom(PendingRequest &req);
    void startNominatimTo  (PendingRequest &req);
    void startOrsRouting   (PendingRequest &req);
    void startOsrmRouting  (PendingRequest &req);
    void nominatimReplyDone(QNetworkReply *reply, PendingRequest &req);
    void orsRoutingReplyDone (QNetworkReply *reply, int requestId);
    void osrmRoutingReplyDone(QNetworkReply *reply, int requestId);
    void emitHaversine(const PendingRequest &req);

    QString adresseText(const Adresse &a) const;

    QNetworkAccessManager               *m_nam;
    QString                              m_apiKey;
    QMap<QNetworkReply*, PendingRequest> m_pending;
};
