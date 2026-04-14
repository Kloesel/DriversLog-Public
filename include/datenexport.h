#pragma once
// datenexport.h
// Changelog v1.1.01 – CSV-Export um Adressen-Auswahl erweitert
//   • Checkbox "Adressen exportieren" / "Fahrten exportieren"
//   • Fahrten-Filtergruppe wird bei Bedarf deaktiviert
//   • ExportDialog nicht mehr benötigt

#include <QWidget>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QLabel>
#include "database.h"
#include "version.h"

// Gruppierter Eintrag: gleiche Start+Zieladresse → km summiert
struct FahrtGruppe {
    QString startAdresse;
    QString zielAdresse;
    double  kmGesamt   = 0.0;
    int     anzahl     = 0;
};

class DatenExport : public QWidget
{
    Q_OBJECT
public:
    explicit DatenExport(Database *db, QWidget *parent = nullptr);

public slots:
    void refresh();

private slots:
    void onExportCsv();
    void onExportPdf();
    void updateUi();

private:
    QVector<Fahrt>  getFilteredFahrten();
    QString         buildCsvFahrten(const QVector<Fahrt> &fahrten);
    QString         buildCsvAdressen(const QVector<Adresse> &adressen);
    QString         buildCsvZusammenfassung(const QVector<FahrtGruppe> &gruppen);
    QString         buildHtmlFahrtenTabelle(const QVector<Fahrt> &fahrten, double &totalKm);
    QString         buildHtmlZusammenfassung(const QVector<FahrtGruppe> &gruppen);
    QVector<FahrtGruppe> gruppiertNachStrecke(const QVector<Fahrt> &fahrten);
    void            generatePdf(const QString &filePath, const QVector<Fahrt> &fahrten,
                                const QVector<Adresse> &adressen = {});

    Database   *m_db;

    // Auswahl
    QCheckBox  *m_chkFahrten;
    QCheckBox  *m_chkZusammenfassung;
    QCheckBox  *m_chkAdressen;

    // Fahrten-Filter
    QGroupBox  *m_grpFilter;
    QComboBox  *m_monatCb;
    QSpinBox   *m_jahrSpin;

    QLabel     *m_previewLabel;
};
