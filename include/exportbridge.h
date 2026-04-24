#pragma once
// exportbridge.h
// Android-Export-Bridge: CSV und PDF via Share-Intent
// Nur auf Android verwendet (nur in android{} Block im .pro eingebunden).

#include <QObject>
#include <QVariantList>
#include <QJniObject>
#include "database.h"
#include "datenexport.h"   // FahrtGruppe

class ExportBridge : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QVariantList monatModel  READ monatModel  CONSTANT)
    Q_PROPERTY(int          initMonat   READ initMonat   CONSTANT)
    Q_PROPERTY(int          initJahr    READ initJahr    CONSTANT)
    Q_PROPERTY(bool         mehrerefahrer READ mehrerefahrer CONSTANT)
    Q_PROPERTY(bool         busy        READ busy        NOTIFY busyChanged)

public:
    explicit ExportBridge(Database *db, QObject *parent = nullptr);

    QVariantList monatModel()   const { return m_monatModel; }
    int          initMonat()    const { return m_initMonat;  }
    int          initJahr()     const { return m_initJahr;   }
    bool         mehrerefahrer()const { return m_mehrerefahrer; }
    bool         busy()         const { return m_busy;       }

    Q_INVOKABLE void exportCsv(int monat, int jahr,
                               bool fahrten, bool zusammenfassung, bool adressen);
    Q_INVOKABLE void exportPdf(int monat, int jahr,
                               bool fahrten, bool zusammenfassung, bool adressen);
    Q_INVOKABLE void reset() { m_busy = false; emit busyChanged(); }

signals:
    void busyChanged();
    void snackbarRequested(QString text, int ms);
    void closeRequested();

private:
    void setStatus(const QString &text, bool busy = false);
    bool writeAndShare   (const QString &fileName, const QString &content,
                          const QString &mimeType);
    bool writeAndSharePdf(const QString &fileName, const QString &html);
    void shareUri        (const QJniObject &uri, const QString &mimeType);

    // Hilfsmethoden (analog DatenExport, aber statisch nutzbar)
    QString buildCsvFahrten       (const QVector<Fahrt> &fahrten);
    QString buildCsvAdressen      (const QVector<Adresse> &adressen);
    QString buildCsvZusammenfassung(const QVector<FahrtGruppe> &gruppen);
    QString buildHtmlFahrtenTabelle(const QVector<Fahrt> &fahrten, double &totalKm);
    QString buildHtmlZusammenfassung(const QVector<FahrtGruppe> &gruppen);
    QVector<FahrtGruppe> gruppiertNachStrecke(const QVector<Fahrt> &fahrten);

    Database     *m_db;
    QVariantList  m_monatModel;
    int           m_initMonat;
    int           m_initJahr;
    bool          m_mehrerefahrer;
    QString       m_lastFilePath;
    bool          m_busy = false;
};

