#pragma once
#include <QObject>
#include <QSqlDatabase>
#include <QVector>
#include "models.h"

class Database : public QObject
{
    Q_OBJECT
public:
    explicit Database(QObject *parent = nullptr);
    ~Database();

    bool open(const QString &path);
    void close();
    bool isOpen() const;
    QString lastError() const;

    /** Gibt die interne QSqlDatabase-Verbindung zurück (für DatabaseSync). */
    QSqlDatabase sqlDatabase() const { return m_db; }

    // Fahrten
    QVector<Fahrt>  getAllFahrten(int monat = 0, int jahr = 0,
                                  const QString &sortField = "datum",
                                  const QString &sortDir   = "DESC");
    bool            insertFahrt(Fahrt &fahrt);
    bool            updateFahrt(const Fahrt &fahrt);
    bool            deleteFahrt(int id);

    // Adressen
    QVector<Adresse> getAllAdressen();
    bool             insertAdresse(Adresse &adresse);
    bool             updateAdresse(const Adresse &adresse);
    bool             updateAdresseCoords(int id, double lat, double lon);
    bool             deleteAdresse(int id);
    bool             adresseInFahrtenUsed(int id);  // SQL statt alle Fahrten laden
    // Gibt ID einer Adresse mit gleicher Strasse+HNr+PLZ+Ort zurück (0 = kein Duplikat).
    // excludeId: eigene ID bei Update ausschließen.
    int              findDuplicateAdresseId(const QString &strasse, const QString &hausnummer,
                                            const QString &plz,     const QString &ort,
                                            int excludeId = 0);
    Adresse          getAdresseById(int id);

    // Fahrer
    QVector<Fahrer>  getAllFahrer();
    bool             insertFahrer(Fahrer &fahrer);
    bool             updateFahrer(const Fahrer &fahrer);
    bool             deleteFahrer(int id);
    Fahrer           getFahrerById(int id);
    int              getDefaultFahrerId();  // ID des "unbekannt"-Fahrers

    // Einstellungen
    Einstellungen    getEinstellungen();
    bool             saveEinstellungen(const Einstellungen &e);

    // Export helper
    QVector<Fahrt>   getFahrtenForExport(int monat, int jahr);
    Fahrt            getFahrtById(int id);

    static QString defaultDbPath();

private:
    QSqlDatabase m_db;
    bool createTables();
    bool migrate();
};
