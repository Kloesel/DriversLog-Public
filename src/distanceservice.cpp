#include "distanceservice.h"
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QtMath>
#include <QDebug>

// Geocoding: Nominatim (OpenStreetMap) – kostenlos, kein Key erforderlich
//   Nutzungsbedingungen: max. 1 Anfrage/Sek., User-Agent pflicht
//   https://nominatim.org/release-docs/develop/api/Search/
static const QString NOMINATIM_URL = "https://nominatim.openstreetmap.org/search";

// Routing Option 1: OSRM (Open Source Routing Machine) – kostenlos, kein Key
//   Demo-Server, Fair Use, für persönliche Nutzung geeignet
//   https://project-osrm.org/
static const QString OSRM_DIRECTIONS = "https://router.project-osrm.org/route/v1/driving/%1,%2;%3,%4?overview=false";

// Routing Option 2: OpenRouteService – kostenlos bis 500 Req/Tag, Key erforderlich
//   Registrierung: https://openrouteservice.org/
static const QString ORS_DIRECTIONS = "https://api.openrouteservice.org/v2/directions/driving-car";

// Ablauf:
//   Koordinaten vorhanden?  → direkt zu Routing
//   Koordinaten fehlen?     → Nominatim Geocoding
//                              dann: ORS Routing (wenn Key gesetzt)
//                                    OSRM Routing (kein Key nötig)
//                                    Haversine x 1.3 (Fallback bei Netzwerkfehler)

DistanceService::DistanceService(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
    connect(m_nam, &QNetworkAccessManager::finished,
            this,  &DistanceService::onReplyFinished);
}

// ── Oeffentliche Schnittstelle ────────────────────────────────────────────────

void DistanceService::requestDistance(int requestId, const Adresse &from, const Adresse &to)
{
    bool fromHasCoords = (from.latitude != 0.0 || from.longitude != 0.0);
    bool toHasCoords   = (to.latitude   != 0.0 || to.longitude   != 0.0);

    PendingRequest req;
    req.requestId = requestId;
    req.from = from;
    req.to   = to;

    if (fromHasCoords) { req.fromLat = from.latitude;  req.fromLon = from.longitude; }
    if (toHasCoords)   { req.toLat   = to.latitude;    req.toLon   = to.longitude;   }

    if (!fromHasCoords) {
        req.stage = NominatimFrom;
        startNominatimFrom(req);
    } else if (!toHasCoords) {
        req.stage = NominatimTo;
        startNominatimTo(req);
    } else if (!m_apiKey.isEmpty()) {
        req.stage = OrsRouting;
        startOrsRouting(req);
    } else {
        req.stage = OsrmRouting;
        startOsrmRouting(req);
    }
}

// ── Nominatim Geocoding ───────────────────────────────────────────────────────

void DistanceService::startNominatimFrom(PendingRequest &req)
{
    QUrl url(NOMINATIM_URL);
    QUrlQuery q;
    q.addQueryItem("q",      adresseText(req.from));
    q.addQueryItem("format", "json");
    q.addQueryItem("limit",  "1");
    url.setQuery(q);

    QNetworkRequest netReq(url);
    netReq.setRawHeader("User-Agent",     "Fahrtenbuch/1.1 (Qt)");
    netReq.setRawHeader("Accept-Language","de,en");
    QNetworkReply *reply = m_nam->get(netReq);
    m_pending.insert(reply, req);
}

void DistanceService::startNominatimTo(PendingRequest &req)
{
    QUrl url(NOMINATIM_URL);
    QUrlQuery q;
    q.addQueryItem("q",      adresseText(req.to));
    q.addQueryItem("format", "json");
    q.addQueryItem("limit",  "1");
    url.setQuery(q);

    QNetworkRequest netReq(url);
    netReq.setRawHeader("User-Agent",     "Fahrtenbuch/1.1 (Qt)");
    netReq.setRawHeader("Accept-Language","de,en");
    QNetworkReply *reply = m_nam->get(netReq);
    m_pending.insert(reply, req);
}

// ── OSRM Routing ──────────────────────────────────────────────────────────────

void DistanceService::startOsrmRouting(PendingRequest &req)
{
    // OSRM erwartet: lon,lat (Längengrad zuerst)
    QString url = OSRM_DIRECTIONS
        .arg(req.fromLon, 0, 'f', 6)
        .arg(req.fromLat, 0, 'f', 6)
        .arg(req.toLon,   0, 'f', 6)
        .arg(req.toLat,   0, 'f', 6);

    QNetworkRequest netReq{QUrl(url)};
    netReq.setRawHeader("User-Agent", "Fahrtenbuch/1.1 (Qt)");
    QNetworkReply *reply = m_nam->get(netReq);
    m_pending.insert(reply, req);
}

// ── ORS Routing ───────────────────────────────────────────────────────────────

void DistanceService::startOrsRouting(PendingRequest &req)
{
    // ORS erwartet lon,lat (Laengengrad zuerst), mind. 5 Dezimalstellen
    QString start = QString("%1,%2").arg(req.fromLon, 0, 'f', 6).arg(req.fromLat, 0, 'f', 6);
    QString end   = QString("%1,%2").arg(req.toLon,   0, 'f', 6).arg(req.toLat,   0, 'f', 6);

    QUrl url(ORS_DIRECTIONS);
    QUrlQuery q;
    q.addQueryItem("api_key",    m_apiKey);
    q.addQueryItem("start",      start);
    q.addQueryItem("end",        end);
    q.addQueryItem("preference", QStringLiteral("fastest"));
    url.setQuery(q);

    QNetworkRequest netReq(url);
    netReq.setRawHeader("Accept", "application/json, application/geo+json");
    QNetworkReply *reply = m_nam->get(netReq);
    m_pending.insert(reply, req);
}

// ── Antwort-Verarbeitung ──────────────────────────────────────────────────────

void DistanceService::onReplyFinished(QNetworkReply *reply)
{
    reply->deleteLater();
    if (!m_pending.contains(reply)) return;
    PendingRequest req = m_pending.take(reply);

    if (reply->error() != QNetworkReply::NoError) {
        int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QByteArray body = reply->readAll();
        if (httpStatus == 403) {
            qWarning() << "[ORS] API-Key ungueltig oder Limit erreicht (403) – verwende Haversine";
            emit apiKeyInvalid();   // Signal fuer UI-Hinweis
        } else {
            qWarning() << "Netzwerkfehler:" << reply->errorString()
                       << "| HTTP:" << httpStatus
                       << "| Body:" << body.left(400);
        }
        if (req.fromLat != 0.0 && req.toLat != 0.0)
            emitHaversine(req);
        else
            emit distanceReady(req.requestId, 0.0, false);
        return;
    }

    if (req.stage == NominatimFrom || req.stage == NominatimTo) {
        nominatimReplyDone(reply, req);
    } else if (req.stage == OsrmRouting) {
        osrmRoutingReplyDone(reply, req.requestId);
    } else {
        orsRoutingReplyDone(reply, req.requestId);
    }
}

void DistanceService::nominatimReplyDone(QNetworkReply *reply, PendingRequest &req)
{
    QByteArray data = reply->readAll();
    QJsonArray arr  = QJsonDocument::fromJson(data).array();

    if (arr.isEmpty()) {
        qWarning() << "Nominatim: kein Ergebnis fuer"
                   << (req.stage == NominatimFrom ? adresseText(req.from) : adresseText(req.to));
        // Weiter mit Haversine falls die andere Seite schon koordiniert ist
        if (req.fromLat != 0.0 && req.toLat != 0.0) {
            if (!m_apiKey.isEmpty()) {
                req.stage = OrsRouting;
                startOrsRouting(req);
            } else {
                req.stage = OsrmRouting;
                startOsrmRouting(req);
            }
        } else {
            emitHaversine(req);
        }
        return;
    }

    QJsonObject hit = arr[0].toObject();
    double lat = hit["lat"].toString().toDouble();
    double lon = hit["lon"].toString().toDouble();

    if (req.stage == NominatimFrom) {
        req.fromLat = lat;  req.fromLon = lon;
        if (req.from.id > 0) emit coordsResolved(req.from.id, lat, lon);

        if (req.toLat == 0.0 && req.toLon == 0.0) {
            req.stage = NominatimTo;
            startNominatimTo(req);
        } else if (!m_apiKey.isEmpty()) {
            req.stage = OrsRouting;
            startOrsRouting(req);
        } else {
            req.stage = OsrmRouting;
            startOsrmRouting(req);
        }
    } else {
        req.toLat = lat;  req.toLon = lon;
        if (req.to.id > 0) emit coordsResolved(req.to.id, lat, lon);

        if (!m_apiKey.isEmpty()) {
            req.stage = OrsRouting;
            startOrsRouting(req);
        } else {
            req.stage = OsrmRouting;
            startOsrmRouting(req);
        }
    }
}

void DistanceService::orsRoutingReplyDone(QNetworkReply *reply, int requestId)
{
    QByteArray data = reply->readAll();
    QJsonObject root = QJsonDocument::fromJson(data).object();

    // GeoJSON-Format: features[0].properties.summary.distance (Meter)
    QJsonArray features = root["features"].toArray();
    if (!features.isEmpty()) {
        double meters = features[0].toObject()["properties"].toObject()
                        ["summary"].toObject()["distance"].toDouble();
        if (meters > 0.0) {
            emit distanceReady(requestId, meters / 1000.0, true);
            return;
        }
    }

    // Aelteres Format: routes[0].summary.distance
    QJsonArray routes = root["routes"].toArray();
    if (!routes.isEmpty()) {
        double meters = routes[0].toObject()["summary"].toObject()["distance"].toDouble();
        if (meters > 0.0) {
            emit distanceReady(requestId, meters / 1000.0, true);
            return;
        }
    }

    qWarning() << "ORS Routing: kein gueltiges Ergebnis -" << data.left(300);
    emit distanceReady(requestId, 0.0, false);
}

void DistanceService::osrmRoutingReplyDone(QNetworkReply *reply, int requestId)
{
    QByteArray data = reply->readAll();
    QJsonObject root = QJsonDocument::fromJson(data).object();

    // OSRM-Format: routes[0].distance (Meter, direkt – kein summary-Objekt)
    QJsonArray routes = root["routes"].toArray();
    if (!routes.isEmpty()) {
        double meters = routes[0].toObject()["distance"].toDouble();
        if (meters > 0.0) {
            emit distanceReady(requestId, meters / 1000.0, true);
            return;
        }
    }

    qWarning() << "OSRM Routing: kein gueltiges Ergebnis -" << data.left(300);
    emit distanceReady(requestId, 0.0, false);
}

// ── Hilfsfunktionen ───────────────────────────────────────────────────────────

void DistanceService::emitHaversine(const PendingRequest &req)
{
    double km = haversine(req.fromLat, req.fromLon, req.toLat, req.toLon);
    qDebug() << "Haversine:" << km * 1.3 << "km";
    emit distanceReady(req.requestId, km * 1.3, true);
}

QString DistanceService::adresseText(const Adresse &a) const
{
    QStringList parts;
    if (!a.strasse.isEmpty()) {
        QString str = a.strasse;
        if (!a.hausnummer.isEmpty()) str += " " + a.hausnummer;
        parts << str;
    } else if (!a.bezeichnung.isEmpty()) {
        parts << a.bezeichnung;
    }
    if (!a.plz.isEmpty() || !a.ort.isEmpty())
        parts << (a.plz + " " + a.ort).trimmed();
    if (!a.land.isEmpty())
        parts << a.land;
    return parts.join(", ");
}

double DistanceService::haversine(double lat1, double lon1, double lat2, double lon2)
{
    const double R = 6371.0;
    double dLat = qDegreesToRadians(lat2 - lat1);
    double dLon = qDegreesToRadians(lon2 - lon1);
    double a = qSin(dLat/2)*qSin(dLat/2) +
               qCos(qDegreesToRadians(lat1))*qCos(qDegreesToRadians(lat2))*
               qSin(dLon/2)*qSin(dLon/2);
    double c = 2 * qAtan2(qSqrt(a), qSqrt(1-a));
    return R * c;
}
