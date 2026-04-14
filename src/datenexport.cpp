// datenexport.cpp
// Changelog v1.1.01 – CSV-Export um Adressen-Auswahl erweitert

#include "datenexport.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QDate>
#include <QTextStream>
#include <QPrinter>
#include <QPageLayout>
#include <QPageSize>
#include <QTextDocument>
#include <QDesktopServices>
#include <QUrl>
#include <QApplication>
#include <QMap>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#  include <QStringConverter>
#else
#  include <QTextCodec>
#endif

// ─────────────────────────────────────────────────────────────────────────────
DatenExport::DatenExport(Database *db, QWidget *parent)
    : QWidget(parent), m_db(db)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16,16,16,16);
    layout->setSpacing(16);

    // ── Was exportieren? ──────────────────────────────────────────────────
    auto *grpWas    = new QGroupBox(tr("Was soll exportiert werden?"), this);
    auto *wasLayout = new QVBoxLayout(grpWas);
    m_chkFahrten      = new QCheckBox(tr("Fahrten (Einzelauflistung)"),          grpWas);
    m_chkZusammenfassung = new QCheckBox(tr("Fahrten (Zusammenfassung nach Strecke)"), grpWas);
    m_chkAdressen     = new QCheckBox(tr("Adressen"),                            grpWas);
    m_chkFahrten->setChecked(true);
    m_chkZusammenfassung->setChecked(true);
    m_chkAdressen->setChecked(false);
    wasLayout->addWidget(m_chkFahrten);
    wasLayout->addWidget(m_chkZusammenfassung);
    wasLayout->addWidget(m_chkAdressen);
    layout->addWidget(grpWas);

    // ── Fahrten-Filter ────────────────────────────────────────────────────
    m_grpFilter    = new QGroupBox(tr("Zeitraum-Filter (Fahrten)"), this);
    auto *grpLay   = new QFormLayout(m_grpFilter);

    m_monatCb  = new QComboBox(this);
    m_jahrSpin = new QSpinBox(this);

    m_monatCb->addItem(tr("(alle Monate)"), 0);
    QStringList months;
    months << tr("Januar") << tr("Februar") << tr("März") << tr("April") << tr("Mai") << tr("Juni")
           << tr("Juli") << tr("August") << tr("September") << tr("Oktober") << tr("November") << tr("Dezember");
    for (int i = 0; i < 12; i++)
        m_monatCb->addItem(months[i], i + 1);

    m_jahrSpin->setRange(0, 2099);
    m_jahrSpin->setValue(QDate::currentDate().year());
    m_jahrSpin->setSpecialValueText(tr("(alle Jahre)"));

    grpLay->addRow(tr("Monat:"), m_monatCb);
    grpLay->addRow(tr("Jahr:"),  m_jahrSpin);
    layout->addWidget(m_grpFilter);

    // ── Vorschau-Label ────────────────────────────────────────────────────
    m_previewLabel = new QLabel(tr("Waehlen Sie einen Filter und exportieren Sie die Daten."), this);
    m_previewLabel->setWordWrap(true);
    layout->addWidget(m_previewLabel);

    // ── Buttons ───────────────────────────────────────────────────────────
    auto *btnLay = new QHBoxLayout();
    auto *csvBtn = new QPushButton(tr("📥  Export als CSV"), this);
    auto *pdfBtn = new QPushButton(tr("📄  Export als PDF"), this);
    csvBtn->setMinimumHeight(40);
    pdfBtn->setMinimumHeight(40);
    pdfBtn->setToolTip(tr("PDF-Export ist nur für Fahrten verfügbar"));
    btnLay->addWidget(csvBtn);
    btnLay->addWidget(pdfBtn);
    layout->addLayout(btnLay);

    layout->addStretch();

    // ── Filter einmalig aus QSettings initialisieren ──────────────────────
    {
        Einstellungen e = m_db->getEinstellungen();
        int monatIdx = m_monatCb->findData(e.filterMonat);
        if (monatIdx >= 0) m_monatCb->setCurrentIndex(monatIdx);
        if (e.filterJahr > 0) m_jahrSpin->setValue(e.filterJahr);
    }

    // ── Verbindungen ──────────────────────────────────────────────────────
    connect(csvBtn,        SIGNAL(clicked()),              this, SLOT(onExportCsv()));
    connect(pdfBtn,        SIGNAL(clicked()),              this, SLOT(onExportPdf()));
    connect(m_chkFahrten,         SIGNAL(toggled(bool)), this, SLOT(updateUi()));
    connect(m_chkZusammenfassung, SIGNAL(toggled(bool)), this, SLOT(updateUi()));
    connect(m_chkAdressen,        SIGNAL(toggled(bool)), this, SLOT(updateUi()));
    connect(m_monatCb,     SIGNAL(currentIndexChanged(int)), this, SLOT(refresh()));
    connect(m_jahrSpin,    SIGNAL(valueChanged(int)),      this, SLOT(refresh()));

    updateUi();
}

// ─────────────────────────────────────────────────────────────────────────────
void DatenExport::updateUi()
{
    // Filter relevant wenn Fahrten-Einzelliste oder Zusammenfassung gewählt
    m_grpFilter->setEnabled(m_chkFahrten->isChecked() || m_chkZusammenfassung->isChecked());

    // Vorschau aktualisieren
    refresh();
}

// ─────────────────────────────────────────────────────────────────────────────
void DatenExport::refresh()
{
    // Nur Vorschau neu berechnen – Controls NICHT aus QSettings überschreiben.
    // (Initialisierung einmalig im Konstruktor; hier würden Änderungen sonst
    //  sofort rückgängig gemacht, weil jede Control-Änderung refresh() auslöst.)

    QStringList info;

    if (m_chkFahrten->isChecked() || m_chkZusammenfassung->isChecked()) {
        QVector<Fahrt> fahrten = getFilteredFahrten();
        double total = 0;
        for (const Fahrt &f : fahrten) total += f.entfernung;
        if (m_chkFahrten->isChecked())
            info << tr("Fahrten: %1 | Gesamt: %2 km")
                    .arg(fahrten.size()).arg(total, 0, 'f', 1);
        if (m_chkZusammenfassung->isChecked()) {
            auto gruppen = gruppiertNachStrecke(fahrten);
            info << tr("Zusammenfassung: %1 Strecken").arg(gruppen.size());
        }
    }

    if (m_chkAdressen->isChecked()) {
        int count = m_db->getAllAdressen().size();
        info << tr("Adressen: %1").arg(count);
    }

    if (info.isEmpty())
        m_previewLabel->setText(tr("Bitte mindestens eine Option auswählen."));
    else
        m_previewLabel->setText(info.join("   |   "));
}

// ─────────────────────────────────────────────────────────────────────────────
QVector<Fahrt> DatenExport::getFilteredFahrten()
{
    int monat = m_monatCb->currentData().toInt();
    int jahr  = m_jahrSpin->value();
    return m_db->getFahrtenForExport(monat, jahr);
}


// ─────────────────────────────────────────────────────────────────────────────
// Alle Fahrten mit gleicher Start- und Zieladresse zusammenfassen, km summieren
// Sortierung: km absteigend
QVector<FahrtGruppe> DatenExport::gruppiertNachStrecke(const QVector<Fahrt> &fahrten)
{
    // Key: "Start|||Ziel"
    QMap<QString, FahrtGruppe> map;
    for (const Fahrt &f : fahrten) {
        QString key = f.startAdresse + "|||" + f.zielAdresse;
        if (!map.contains(key)) {
            FahrtGruppe g;
            g.startAdresse = f.startAdresse;
            g.zielAdresse  = f.zielAdresse;
            map[key] = g;
        }
        map[key].kmGesamt += f.entfernung;
        map[key].anzahl   += 1;
    }

    QVector<FahrtGruppe> result = map.values().toVector();
    // Absteigend nach km sortieren
    std::sort(result.begin(), result.end(), [](const FahrtGruppe &a, const FahrtGruppe &b){
        return a.kmGesamt > b.kmGesamt;
    });
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
QString DatenExport::buildCsvZusammenfassung(const QVector<FahrtGruppe> &gruppen)
{
    auto esc = [](const QString &s) -> QString {
        return "\"" + QString(s).replace("\"", "\"\"")+"\"";
    };

    QString out;
    out += "\xEF\xBB\xBF";
    out += tr("Startadresse") + ";" + tr("Zieladresse") + ";" + tr("Anzahl Fahrten") + ";" + tr("Gesamt (km)") + "\n";

    double total = 0;
    for (const FahrtGruppe &g : gruppen) {
        out += esc(g.startAdresse) + ";"
             + esc(g.zielAdresse)  + ";"
             + QString::number(g.anzahl) + ";"
             + QString::number(g.kmGesamt, 'f', 1) + "\n";
        total += g.kmGesamt;
    }
    out += ";;" + tr("Gesamt") + ";" + QString::number(total, 'f', 1) + "\n";
    return out;
}

// ─────────────────────────────────────────────────────────────────────────────
QString DatenExport::buildHtmlFahrtenTabelle(const QVector<Fahrt> &fahrten, double &totalKm)
{
    totalKm = 0;
    QString html;
    Einstellungen eHtml = m_db->getEinstellungen();
    bool mitFahrerHtml = eHtml.mehrerefahrer;
    int colSpanGesamt = mitFahrerHtml ? 4 : 3;

    html += "<table border='1' cellpadding='4' cellspacing='0' "
            "style='font-family:Arial;font-size:10pt;width:100%;border-collapse:collapse;'>"
            "<tr style='background-color:#2E75B6;color:white;font-weight:bold;'>"
            "<th>" + tr("Datum") + "</th>";
    if (mitFahrerHtml) html += "<th>" + tr("Fahrer") + "</th>";
    html += "<th>" + tr("Startadresse") + "</th><th>" + tr("Zieladresse") + "</th>"
            "<th>" + tr("Entfernung (km)") + "</th><th>" + tr("Hin & Zurück") + "</th><th>" + tr("Bemerkung") + "</th>"
            "</tr>";
    bool alt = false;
    for (const Fahrt &f : fahrten) {
        totalKm += f.entfernung;
        QString bg = alt ? "#f0f0f0" : "#ffffff"; alt = !alt;
        html += QString("<tr style='background-color:%1;'><td>%2</td>")
                .arg(bg, f.datum.toString("dd.MM.yyyy"));
        if (mitFahrerHtml)
            html += QString("<td>%1</td>").arg(f.fahrerName.toHtmlEscaped());
        html += QString("<td>%1</td><td>%2</td>"
                        "<td align='right'>%3</td><td align='center'>%4</td><td>%5</td></tr>")
                .arg(f.startAdresse.toHtmlEscaped())
                .arg(f.zielAdresse.toHtmlEscaped())
                .arg(QString::number(f.entfernung, 'f', 1))
                .arg(f.hinUndZurueck ? tr("Ja") : tr("Nein"))
                .arg(f.bemerkung.toHtmlEscaped());
    }
    html += QString("<tr style='font-weight:bold;background-color:#BDD7EE;'>"
                    "<td colspan='%1' align='right'><b>"+tr("Gesamt")+":</b></td>"
                    "<td align='right'><b>%2 km</b></td><td colspan='2'></td></tr>")
            .arg(colSpanGesamt)
            .arg(QString::number(totalKm, 'f', 1));
    html += "</table>";
    return html;
}

// ─────────────────────────────────────────────────────────────────────────────
QString DatenExport::buildHtmlZusammenfassung(const QVector<FahrtGruppe> &gruppen)
{
    QString html;
    html += "<h3 style='font-family:Arial;margin-top:20px;'>" + tr("Zusammenfassung nach Strecke") + "</h3>";
    html += "<table border='1' cellpadding='4' cellspacing='0' "
            "style='font-family:Arial;font-size:10pt;width:100%;border-collapse:collapse;'>"
            "<tr style='background-color:#2E75B6;color:white;font-weight:bold;'>"
            "<th>"+tr("Startadresse")+"</th><th>"+tr("Zieladresse")+"</th>"
            "<th>"+tr("Anzahl Fahrten")+"</th><th>"+tr("Gesamt (km)")+"</th>"
            "</tr>";
    double total = 0;
    bool alt = false;
    for (const FahrtGruppe &g : gruppen) {
        total += g.kmGesamt;
        QString bg = alt ? "#f0f0f0" : "#ffffff"; alt = !alt;
        html += QString("<tr style='background-color:%1;'>"
                        "<td>%2</td><td>%3</td>"
                        "<td align='center'>%4</td><td align='right'>%5 km</td></tr>")
                .arg(bg)
                .arg(g.startAdresse.toHtmlEscaped())
                .arg(g.zielAdresse.toHtmlEscaped())
                .arg(g.anzahl)
                .arg(QString::number(g.kmGesamt, 'f', 1));
    }
    html += QString("<tr style='font-weight:bold;background-color:#BDD7EE;'>"
                    "<td colspan='3' align='right'><b>"+tr("Gesamt")+":</b></td>"
                    "<td align='right'><b>%1 km</b></td></tr>")
            .arg(QString::number(total, 'f', 1));
    html += "</table>";
    return html;
}

// ─────────────────────────────────────────────────────────────────────────────
QString DatenExport::buildCsvFahrten(const QVector<Fahrt> &fahrten)
{
    auto esc = [](const QString &s) -> QString {
        return "\"" + QString(s).replace("\"", "\"\"") + "\"";
    };

    QString out;
    out += "\xEF\xBB\xBF";
    Einstellungen e = m_db->getEinstellungen();
    bool mitFahrer = e.mehrerefahrer;
    if (mitFahrer)
        out += tr("Datum")+";"+tr("Fahrer")+";"+tr("Startadresse")+";"+tr("Zieladresse")+";"+tr("Entfernung (km)")+";"+tr("Hin und Zurück")+";"+tr("Bemerkung")+"\n";
    else
        out += tr("Datum")+";"+tr("Startadresse")+";"+tr("Zieladresse")+";"+tr("Entfernung (km)")+";"+tr("Hin und Zurück")+";"+tr("Bemerkung")+"\n";

    for (const Fahrt &f : fahrten) {
        out += f.datum.toString("dd.MM.yyyy") + ";";
        if (mitFahrer) out += esc(f.fahrerName) + ";";
        out += esc(f.startAdresse)  + ";"
             + esc(f.zielAdresse)   + ";"
             + QString::number(f.entfernung, 'f', 1) + ";"
             + (f.hinUndZurueck ? tr("Ja") : tr("Nein")) + ";"
             + esc(f.bemerkung)     + "\n";
    }
    return out;
}

// ─────────────────────────────────────────────────────────────────────────────
QString DatenExport::buildCsvAdressen(const QVector<Adresse> &adressen)
{
    auto esc = [](const QString &s) -> QString {
        QString e = s;
        e.replace("\"", "\"\"");
        if (e.contains(';') || e.contains(',') || e.contains('\n'))
            return "\"" + e + "\"";
        return e;
    };

    QString out;
    out += "\xEF\xBB\xBF";
    out += tr("Bezeichnung")+";"+tr("Strasse")+";"+tr("Hausnr")+";"+tr("PLZ")+";"+tr("Ort")+";"+tr("Land")+"\n";

    for (const Adresse &a : adressen) {
        out += esc(a.bezeichnung) + ";"
             + esc(a.strasse)     + ";"
             + esc(a.hausnummer)  + ";"
             + esc(a.plz)         + ";"
             + esc(a.ort)         + ";"
             + esc(a.land)        + "\n";
    }
    return out;
}

// ─────────────────────────────────────────────────────────────────────────────
void DatenExport::onExportCsv()
{
    bool doFahrten         = m_chkFahrten->isChecked();
    bool doZusammenfassung = m_chkZusammenfassung->isChecked();
    bool doAdressen        = m_chkAdressen->isChecked();

    if (!doFahrten && !doZusammenfassung && !doAdressen) {
        QMessageBox::information(this, tr("Export"),
            tr("Bitte mindestens eine Option auswählen."));
        return;
    }

    QString stamp = QDate::currentDate().toString("yyyyMMdd");

    // ── Dateiname: wie PDF — sprechend je nach Auswahl ────────────────────
    QString defaultName;
    if (doFahrten || doZusammenfassung) {
        bool nurZusammenfassung = !doFahrten && doZusammenfassung && !doAdressen;
        defaultName = nurZusammenfassung
            ? QString("Fahrtenbuch_Zusammenfassung_%1.csv").arg(stamp)
            : QString("Fahrtenbuch_%1.csv").arg(stamp);
    } else {
        defaultName = QString("Adressen_%1.csv").arg(stamp);
    }

    QApplication::restoreOverrideCursor();
    QString path = QFileDialog::getSaveFileName(this, tr("CSV speichern"),
        defaultName, tr("CSV-Dateien (*.csv)"));
    if (path.isEmpty()) return;

    QApplication::setOverrideCursor(Qt::WaitCursor);

    // ── Inhalt zusammenbauen ──────────────────────────────────────────────
    QString csvContent;
    csvContent += "\xEF\xBB\xBF";   // UTF-8 BOM (einmalig am Anfang)

    bool first = true;  // Abstandszeile zwischen Abschnitten

    auto section = [&](const QString &title) {
        if (!first) csvContent += "\n";
        csvContent += "### " + title + " ###\n";
        first = false;
    };

    QVector<Fahrt> fahrten;
    if (doFahrten || doZusammenfassung)
        fahrten = getFilteredFahrten();

    // Aktuellen Filter in QSettings merken
    {
        Einstellungen e = m_db->getEinstellungen();
        e.filterMonat = m_monatCb->currentData().toInt();
        e.filterJahr  = m_jahrSpin->value();
        m_db->saveEinstellungen(e);
    }

    if (doFahrten) {
        if (fahrten.isEmpty()) {
            QApplication::restoreOverrideCursor();
            QMessageBox::information(this, tr("Export"),
                tr("Keine Fahrten für den gewählten Zeitraum gefunden."));
            return;
        }
        // Einzelne Sektionsüberschrift nur wenn mehrere Abschnitte in der Datei
        if (doZusammenfassung || doAdressen) section(tr("Fahrten"));
        // BOM bereits gesetzt — buildCsvFahrten liefert BOM+Header+Zeilen,
        // wir brauchen nur Header+Zeilen
        QString block = buildCsvFahrten(fahrten);
        block.remove("\xEF\xBB\xBF");  // BOM aus Block entfernen
        csvContent += block;
        first = false;
    }

    if (doZusammenfassung) {
        auto gruppen = gruppiertNachStrecke(fahrten);
        if (gruppen.isEmpty() && !doFahrten) {
            QApplication::restoreOverrideCursor();
            QMessageBox::information(this, tr("Export"),
                tr("Keine Fahrten für die Zusammenfassung gefunden."));
            return;
        }
        if (doFahrten || doAdressen) section(tr("Zusammenfassung nach Strecke"));
        QString block = buildCsvZusammenfassung(gruppen);
        block.remove("\xEF\xBB\xBF");
        csvContent += block;
        first = false;
    }

    if (doAdressen) {
        QVector<Adresse> adressen = m_db->getAllAdressen();
        if (adressen.isEmpty()) {
            QApplication::restoreOverrideCursor();
            QMessageBox::information(this, tr("Export"), tr("Keine Adressen vorhanden."));
            return;
        }
        if (doFahrten || doZusammenfassung) section(tr("Adressen"));
        QString block = buildCsvAdressen(adressen);
        block.remove("\xEF\xBB\xBF");
        csvContent += block;
    }

    // ── Datei schreiben ───────────────────────────────────────────────────
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QApplication::restoreOverrideCursor();
        QMessageBox::warning(this, tr("Fehler"),
            tr("CSV konnte nicht erstellt werden:\n%1").arg(path));
        return;
    }
    QTextStream ts(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    ts.setEncoding(QStringConverter::Utf8);
#else
    ts.setCodec("UTF-8");
#endif
    ts << csvContent;
    file.close();

    QApplication::restoreOverrideCursor();
    QMessageBox::information(this, tr("Export abgeschlossen"),
        tr("Erfolgreich exportiert:\n%1").arg(path));
}

// ─────────────────────────────────────────────────────────────────────────────
void DatenExport::onExportPdf()
{
    bool doFahrten  = m_chkFahrten->isChecked();
    bool doAdressen = m_chkAdressen->isChecked();

    if (!doFahrten && !m_chkZusammenfassung->isChecked() && !doAdressen) {
        QMessageBox::information(this, tr("PDF-Export"),
            tr("Bitte mindestens eine Option auswählen."));
        return;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);

    QVector<Fahrt>   fahrten;
    QVector<Adresse> adressen;

    bool doZusammenfassung = m_chkZusammenfassung->isChecked();

    if (doFahrten || doZusammenfassung) {
        fahrten = getFilteredFahrten();

        // Aktuellen Filter in QSettings merken
        Einstellungen eSave = m_db->getEinstellungen();
        eSave.filterMonat = m_monatCb->currentData().toInt();
        eSave.filterJahr  = m_jahrSpin->value();
        m_db->saveEinstellungen(eSave);

        if (fahrten.isEmpty() && !doAdressen) {
            QApplication::restoreOverrideCursor();
            QMessageBox::information(this, tr("Export"),
                tr("Keine Fahrten fuer den gewaehlten Zeitraum."));
            return;
        }
    }
    if (doAdressen) {
        adressen = m_db->getAllAdressen();
    }

    // Dateiname: "Zusammenfassung" wenn nur Zusammenfassung gewählt
    QString stamp = QDate::currentDate().toString("yyyyMM");
    bool nurZusammenfassung = !m_chkFahrten->isChecked()
                           && m_chkZusammenfassung->isChecked()
                           && !m_chkAdressen->isChecked();
    QString defaultName = nurZusammenfassung
        ? QString("Fahrtenbuch_Zusammenfassung_%1.pdf").arg(stamp)
        : QString("Fahrtenbuch_%1.pdf").arg(stamp);
    QApplication::restoreOverrideCursor();
    QString path = QFileDialog::getSaveFileName(this, tr("PDF speichern"), defaultName,
                                                tr("PDF-Dateien (*.pdf)"));
    if (path.isEmpty()) return;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    generatePdf(path, fahrten, adressen);
    QApplication::restoreOverrideCursor();

    QMessageBox::information(this, tr("Export"),
        tr("PDF-Export erfolgreich:\n%1").arg(path));
}

// ─────────────────────────────────────────────────────────────────────────────
void DatenExport::generatePdf(const QString &filePath, const QVector<Fahrt> &fahrten,
                              const QVector<Adresse> &adressen)
{
    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(filePath);
    printer.setPageOrientation(QPageLayout::Landscape);
    printer.setPageSize(QPageSize(QPageSize::A4));

    QString html;
    html += "<html><head><meta charset='utf-8'/></head><body>";
    html += "<h2 style='font-family:Arial;'>" + tr("Driver's Log") + "</h2>";

    int monat = m_monatCb->currentData().toInt();
    int jahr  = m_jahrSpin->value();
    if ((monat > 0 || jahr > 0) && !fahrten.isEmpty()) {
        QStringList months;
        months << "" << tr("Januar") << tr("Februar") << tr("März") << tr("April") << tr("Mai") << tr("Juni")
               << tr("Juli") << tr("August") << tr("September") << tr("Oktober") << tr("November") << tr("Dezember");
        QString period;
        if (monat > 0 && monat <= 12) period += months[monat] + " ";
        if (jahr  > 0)  period += QString::number(jahr);
        html += "<p style='font-family:Arial;'>" + tr("Zeitraum: %1").arg(period.trimmed()) + "</p>";
    }

    // ── Fahrten-Einzeltabelle — nur wenn explizit gewählt ───────────────
    if (m_chkFahrten->isChecked() && !fahrten.isEmpty()) {
        html += "<h3 style='font-family:Arial;margin-top:12px;'>" + tr("Fahrten") + "</h3>";
        double totalKm = 0;
        html += buildHtmlFahrtenTabelle(fahrten, totalKm);
    }

    // ── Zusammenfassung — nur wenn explizit gewählt ───────────────────
    if (m_chkZusammenfassung->isChecked() && !fahrten.isEmpty()) {
        auto gruppen = gruppiertNachStrecke(fahrten);
        html += buildHtmlZusammenfassung(gruppen);
    }

    // ── Adressen-Tabelle ──────────────────────────────────────────────────
    if (!adressen.isEmpty()) {
        if (!fahrten.isEmpty())
            html += "<br/><br/>";  // Abstand zwischen den Tabellen

        html += "<h3 style='font-family:Arial;margin-top:12px;'>" + tr("Adressen") + "</h3>";
        html += "<table border='1' cellpadding='4' cellspacing='0' "
                "style='font-family:Arial;font-size:10pt;width:100%;border-collapse:collapse;'>"
                "<tr style='background-color:#2E75B6;color:white;font-weight:bold;'>"
                "<th>" + tr("Bezeichnung") + "</th><th>" + tr("Straße") + "</th><th>" + tr("Hausnr.") + "</th>"
                "<th>" + tr("PLZ") + "</th><th>" + tr("Ort") + "</th><th>" + tr("Land") + "</th>"
                "</tr>";

        bool alt = false;
        for (const Adresse &a : adressen) {
            QString bg = alt ? "#f0f0f0" : "#ffffff";
            alt = !alt;
            html += QString("<tr style='background-color:%1;'>"
                            "<td>%2</td><td>%3</td><td>%4</td>"
                            "<td>%5</td><td>%6</td><td>%7</td></tr>")
                    .arg(bg)
                    .arg(a.bezeichnung.toHtmlEscaped())
                    .arg(a.strasse.toHtmlEscaped())
                    .arg(a.hausnummer.toHtmlEscaped())
                    .arg(a.plz.toHtmlEscaped())
                    .arg(a.ort.toHtmlEscaped())
                    .arg(a.land.toHtmlEscaped());
        }
        html += QString("<tr style='font-weight:bold;background-color:#BDD7EE;'>"
                        "<td colspan='6'><b>" + tr("%1 Adressen gesamt").arg(adressen.size()) + "</b></td></tr>");
        html += "</table>";
    }

    html += ("<p style='font-family:Arial;font-size:8pt;color:gray;margin-top:16px;'>"
         + tr("Erstellt am %1 | Driver's Log v%2")
               .arg(QDate::currentDate().toString("dd.MM.yyyy"), APP_VERSION)
         + "</p>");
    html += "</body></html>";

    QTextDocument doc;
    doc.setHtml(html);
    doc.print(&printer);
}
