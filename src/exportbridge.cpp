// exportbridge.cpp
// Android-Export: CSV/PDF erzeugen und via Share-Intent teilen.
// Wird nur im Android-Build kompiliert (android{} in .pro).

#include "exportbridge.h"
#include "snackbar.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTextStream>
#include <QTextDocument>
#include <QPagedPaintDevice>
#include <QPdfWriter>
#include <QDate>
#include <QMap>
#include <algorithm>

// JNI für Share-Intent und getExternalFilesDir
#include <QJniObject>
#include <QJniEnvironment>
#include <QtCore/qnativeinterface.h>
#include <QCoreApplication>

// ─── Konstruktor ─────────────────────────────────────────────────────────────

ExportBridge::ExportBridge(Database *db, QObject *parent)
    : QObject(parent), m_db(db)
{
    Einstellungen e = m_db->getEinstellungen();
    m_initMonat      = e.filterMonat;
    m_initJahr       = e.filterJahr > 0 ? e.filterJahr : QDate::currentDate().year();
    m_mehrerefahrer  = e.mehrerefahrer;

    // Monat-Modell
    QVariantMap all; all["id"] = 0; all["text"] = tr("Alle Monate");
    m_monatModel.append(all);
    const QStringList months = {
        tr("Januar"), tr("Februar"), tr("März"),    tr("April"),
        tr("Mai"),    tr("Juni"),    tr("Juli"),     tr("August"),
        tr("September"), tr("Oktober"), tr("November"), tr("Dezember")
    };
    for (int i = 0; i < 12; ++i) {
        QVariantMap m; m["id"] = i+1; m["text"] = months[i];
        m_monatModel.append(m);
    }
}

// ─── Status ──────────────────────────────────────────────────────────────────

void ExportBridge::setStatus(const QString &text, bool busy)
{
    m_busy = busy;
    emit busyChanged();
    if (!text.isEmpty())
        emit snackbarRequested(text, busy ? 2000 : 4000);
}

// ─── Hilfsmethode: Export-Verzeichnis ────────────────────────────────────────


void ExportBridge::exportCsv(int monat, int jahr,
                              bool fahrten, bool zusammenfassung, bool adressen)
{
    qDebug() << "[Export] exportCsv start monat=" << monat << "jahr=" << jahr;
    if (!fahrten && !zusammenfassung && !adressen) {
        setStatus(tr("Bitte mindestens eine Option wählen."));
        return;
    }

    setStatus(tr("Exportiere..."), true);
    qDebug() << "[Export] status set";

    QString stamp = QDate::currentDate().toString("yyyyMMdd");
    QString fileName = (fahrten || zusammenfassung)
        ? QString("Driverslog_%1.csv").arg(stamp)
        : QString("Adressen_%1.csv").arg(stamp);

    QString content;
    content += "\xEF\xBB\xBF";   // UTF-8 BOM
    bool first = true;

    auto section = [&](const QString &title) {
        if (!first) content += "\n";
        content += "### " + title + " ###\n";
        first = false;
    };

    QVector<Fahrt> fahrtenData;
    if (fahrten || zusammenfassung)
        fahrtenData = m_db->getFahrtenForExport(monat, jahr);

    if (fahrten) {
        if (fahrtenData.isEmpty()) {
            setStatus(tr("Keine Fahrten für den gewählten Zeitraum."));
            return;
        }
        if (zusammenfassung || adressen) section(tr("Fahrten"));
        QString block = buildCsvFahrten(fahrtenData);
        block.remove("\xEF\xBB\xBF");
        content += block;
        first = false;
    }

    if (zusammenfassung) {
        auto gruppen = gruppiertNachStrecke(fahrtenData);
        if (gruppen.isEmpty() && !fahrten) {
            setStatus(tr("Keine Fahrten für den gewählten Zeitraum."));
            return;
        }
        if (fahrten || adressen) section(tr("Zusammenfassung"));
        QString block = buildCsvZusammenfassung(gruppen);
        block.remove("\xEF\xBB\xBF");
        content += block;
        first = false;
    }

    if (adressen) {
        auto adressenData = m_db->getAllAdressen();
        if (fahrten || zusammenfassung) section(tr("Adressen"));
        QString block = buildCsvAdressen(adressenData);
        block.remove("\xEF\xBB\xBF");
        content += block;
    }

    if (writeAndShare(fileName, content, "text/csv")) {
        setStatus(tr("CSV gespeichert: %1").arg(QFileInfo(m_lastFilePath).fileName()));
    } else {
        setStatus(tr("Fehler beim Erstellen der CSV-Datei."));
    }
}

// ─── PDF-Export ──────────────────────────────────────────────────────────────

void ExportBridge::exportPdf(int monat, int jahr,
                              bool fahrten, bool zusammenfassung, bool adressen)
{
    if (!fahrten && !zusammenfassung && !adressen) {
        setStatus(tr("Bitte mindestens eine Option wählen."));
        return;
    }

    setStatus(tr("Erstelle PDF..."), true);

    QVector<Fahrt>   fahrtenData;
    QVector<Adresse> adressenData;

    if (fahrten || zusammenfassung)
        fahrtenData = m_db->getFahrtenForExport(monat, jahr);
    if (adressen)
        adressenData = m_db->getAllAdressen();

    if ((fahrten || zusammenfassung) && fahrtenData.isEmpty() && !adressen) {
        setStatus(tr("Keine Fahrten für den gewählten Zeitraum."));
        return;
    }

    // HTML aufbauen
    QString html;
    html += "<html><head><meta charset='utf-8'/>"
            "<style>"
            "body { font-family: Arial; font-size: 10pt; }"
            "h2 { color: #2E75B6; }"
            "h3 { color: #2E75B6; margin-top: 16px; }"
            "</style></head><body>";
    html += "<h2>" + tr("Fahrtenprotokoll") + "</h2>";

    if (monat > 0 || jahr > 0) {
        QStringList months = {"", tr("Januar"), tr("Februar"), tr("März"),
            tr("April"), tr("Mai"), tr("Juni"), tr("Juli"), tr("August"),
            tr("September"), tr("Oktober"), tr("November"), tr("Dezember")};
        QString periode;
        if (monat > 0 && monat <= 12) periode += months[monat] + " ";
        if (jahr  > 0) periode += QString::number(jahr);
        if (!periode.isEmpty())
            html += "<p><b>" + tr("Zeitraum:") + "</b> " + periode.trimmed() + "</p>";
    }

    if (fahrten && !fahrtenData.isEmpty()) {
        double totalKm = 0;
        html += buildHtmlFahrtenTabelle(fahrtenData, totalKm);
    }

    if (zusammenfassung && !fahrtenData.isEmpty()) {
        auto gruppen = gruppiertNachStrecke(fahrtenData);
        html += buildHtmlZusammenfassung(gruppen);
    }

    if (adressen && !adressenData.isEmpty()) {
        html += "<h3>" + tr("Adressen") + "</h3>";
        html += "<table border='1' cellpadding='4' cellspacing='0' "
                "style='font-family:Arial;font-size:10pt;width:100%;border-collapse:collapse;'>"
                "<tr style='background-color:#2E75B6;color:white;font-weight:bold;'>"
                "<th>" + tr("Bezeichnung") + "</th><th>" + tr("Strasse") + "</th>"
                "<th>" + tr("PLZ") + "</th><th>" + tr("Ort") + "</th></tr>";
        bool alt = false;
        for (const Adresse &a : adressenData) {
            QString bg = alt ? "#f0f0f0" : "#ffffff"; alt = !alt;
            html += QString("<tr style='background-color:%1;'><td>%2</td><td>%3 %4</td>"
                            "<td>%5</td><td>%6</td></tr>")
                    .arg(bg)
                    .arg(a.bezeichnung.toHtmlEscaped())
                    .arg(a.strasse.toHtmlEscaped())
                    .arg(a.hausnummer.toHtmlEscaped())
                    .arg(a.plz.toHtmlEscaped())
                    .arg(a.ort.toHtmlEscaped());
        }
        html += "</table>";
    }

    html += "</body></html>";

    QString stamp    = QDate::currentDate().toString("yyyyMM");
    QString fileName = QString("Driverslog_%1.pdf").arg(stamp);

    if (writeAndSharePdf(fileName, html)) {
        setStatus(tr("PDF gespeichert: %1").arg(QFileInfo(m_lastFilePath).fileName()));
    } else {
        setStatus(tr("Fehler beim Erstellen des PDFs."));
    }
}

// ─── Datei in öffentliche Downloads schreiben via MediaStore ─────────────────
// Android 10+ (API 29): MediaStore.Downloads → content:// URI direkt, kein FileProvider nötig

static QJniObject insertMediaStoreDownloads(const QString &fileName,
                                             const QString &mimeType)
{
    QJniObject resolver = QJniObject(QNativeInterface::QAndroidApplication::context().object())
                          .callObjectMethod("getContentResolver",
                                            "()Landroid/content/ContentResolver;");

    QJniObject values("android/content/ContentValues");
    values.callMethod<void>("put",
        "(Ljava/lang/String;Ljava/lang/String;)V",
        QJniObject::fromString("_display_name").object<jstring>(),
        QJniObject::fromString(fileName).object<jstring>());
    values.callMethod<void>("put",
        "(Ljava/lang/String;Ljava/lang/String;)V",
        QJniObject::fromString("mime_type").object<jstring>(),
        QJniObject::fromString(mimeType).object<jstring>());

    // MediaStore.Downloads.EXTERNAL_CONTENT_URI
    QJniObject downloadsUri = QJniObject::callStaticObjectMethod(
        "android/provider/MediaStore$Downloads",
        "getContentUri",
        "(Ljava/lang/String;)Landroid/net/Uri;",
        QJniObject::fromString("external").object<jstring>());

    return resolver.callObjectMethod(
        "insert",
        "(Landroid/net/Uri;Landroid/content/ContentValues;)Landroid/net/Uri;",
        downloadsUri.object(),
        values.object());
}

bool ExportBridge::writeAndShare(const QString &fileName, const QString &content,
                                  const QString &mimeType)
{
    QJniObject uri = insertMediaStoreDownloads(fileName, mimeType);
    if (!uri.isValid()) {
        qWarning() << "[Export] MediaStore insert failed";
        return false;
    }

    // OutputStream via ContentResolver öffnen
    QJniObject resolver = QJniObject(QNativeInterface::QAndroidApplication::context().object())
                          .callObjectMethod("getContentResolver",
                                            "()Landroid/content/ContentResolver;");
    QJniObject os = resolver.callObjectMethod(
        "openOutputStream",
        "(Landroid/net/Uri;)Ljava/io/OutputStream;",
        uri.object());
    if (!os.isValid()) {
        qWarning() << "[Export] openOutputStream failed";
        return false;
    }

    // Bytes schreiben
    QByteArray bytes = content.toUtf8();
    QJniEnvironment env;
    jbyteArray jBytes = env->NewByteArray(bytes.size());
    env->SetByteArrayRegion(jBytes, 0, bytes.size(),
                            reinterpret_cast<const jbyte*>(bytes.constData()));
    os.callMethod<void>("write", "([B)V", jBytes);
    os.callMethod<void>("close");
    env->DeleteLocalRef(jBytes);

    m_lastFilePath = fileName;  // nur Dateiname für Snackbar
    qDebug() << "[Export] CSV written via MediaStore:" << fileName;

    // Intent mit content:// URI teilen
    shareUri(uri, mimeType);
    return true;
}

bool ExportBridge::writeAndSharePdf(const QString &fileName, const QString &html)
{
    // PDF erst in temp-Datei schreiben, dann in MediaStore kopieren
    QString tmpPath = QStandardPaths::writableLocation(
                          QStandardPaths::TempLocation) + "/" + fileName;

    QPdfWriter writer(tmpPath);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setPageOrientation(QPageLayout::Landscape);
    writer.setResolution(150);
    QTextDocument doc;
    doc.setHtml(html);
    doc.setPageSize(QSizeF(writer.width(), writer.height()));
    doc.print(&writer);

    // Temp-Datei in MediaStore Downloads einfügen
    QJniObject uri = insertMediaStoreDownloads(fileName, "application/pdf");
    if (!uri.isValid()) {
        qWarning() << "[Export] MediaStore PDF insert failed";
        return false;
    }

    QJniObject resolver = QJniObject(QNativeInterface::QAndroidApplication::context().object())
                          .callObjectMethod("getContentResolver",
                                            "()Landroid/content/ContentResolver;");
    QJniObject os = resolver.callObjectMethod(
        "openOutputStream",
        "(Landroid/net/Uri;)Ljava/io/OutputStream;",
        uri.object());
    if (!os.isValid()) return false;

    QFile tmp(tmpPath);
    if (tmp.open(QIODevice::ReadOnly)) {
        QByteArray bytes = tmp.readAll();
        tmp.close();
        QJniEnvironment env;
        jbyteArray jBytes = env->NewByteArray(bytes.size());
        env->SetByteArrayRegion(jBytes, 0, bytes.size(),
                                reinterpret_cast<const jbyte*>(bytes.constData()));
        os.callMethod<void>("write", "([B)V", jBytes);
        os.callMethod<void>("close");
        env->DeleteLocalRef(jBytes);
        QFile::remove(tmpPath);
    }

    m_lastFilePath = fileName;
    qDebug() << "[Export] PDF written via MediaStore:" << fileName;

    shareUri(uri, "application/pdf");
    return true;
}

void ExportBridge::shareUri(const QJniObject &uri, const QString &mimeType)
{
    QJniObject activity = QJniObject(QNativeInterface::QAndroidApplication::context().object());

    QJniObject intent("android/content/Intent",
                      "(Ljava/lang/String;)V",
                      QJniObject::fromString("android.intent.action.VIEW").object<jstring>());
    intent.callObjectMethod("setDataAndType",
        "(Landroid/net/Uri;Ljava/lang/String;)Landroid/content/Intent;",
        uri.object(),
        QJniObject::fromString(mimeType).object<jstring>());
    intent.callObjectMethod("addFlags", "(I)Landroid/content/Intent;",
                            jint(0x00000001)); // FLAG_GRANT_READ_URI_PERMISSION

    QJniObject chooser = QJniObject::callStaticObjectMethod(
        "android/content/Intent",
        "createChooser",
        "(Landroid/content/Intent;Ljava/lang/CharSequence;)Landroid/content/Intent;",
        intent.object(),
        QJniObject::fromString(tr("Datei öffnen mit")).object<jstring>());

    activity.callMethod<void>("startActivity",
                              "(Landroid/content/Intent;)V",
                              chooser.object());
}
// ─── CSV/HTML Build-Methoden ─────────────────────────────────────────────────

QVector<FahrtGruppe> ExportBridge::gruppiertNachStrecke(const QVector<Fahrt> &fahrten)
{
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
    std::sort(result.begin(), result.end(), [](const FahrtGruppe &a, const FahrtGruppe &b){
        return a.kmGesamt > b.kmGesamt;
    });
    return result;
}

QString ExportBridge::buildCsvFahrten(const QVector<Fahrt> &fahrten)
{
    auto esc = [](const QString &s) {
        return "\"" + QString(s).replace("\"", "\"\"") + "\"";
    };
    QString out;
    out += "\xEF\xBB\xBF";
    bool mitFahrer = m_db->getEinstellungen().mehrerefahrer;
    if (mitFahrer)
        out += tr("Datum")+";"+tr("Fahrer")+";"+tr("Startadresse")+";"+
               tr("Zieladresse")+";"+tr("Entfernung (km)")+";"+tr("Hin und Zurück")+";"+tr("Bemerkung")+"\n";
    else
        out += tr("Datum")+";"+tr("Startadresse")+";"+tr("Zieladresse")+";"+
               tr("Entfernung (km)")+";"+tr("Hin und Zurück")+";"+tr("Bemerkung")+"\n";

    for (const Fahrt &f : fahrten) {
        out += f.datum.toString("dd.MM.yyyy") + ";";
        if (mitFahrer) out += esc(f.fahrerName) + ";";
        out += esc(f.startAdresse) + ";" + esc(f.zielAdresse) + ";"
             + QString::number(f.entfernung, 'f', 1) + ";"
             + (f.hinUndZurueck ? tr("Ja") : tr("Nein")) + ";"
             + esc(f.bemerkung) + "\n";
    }
    return out;
}

QString ExportBridge::buildCsvAdressen(const QVector<Adresse> &adressen)
{
    auto esc = [](const QString &s) {
        QString e = s; e.replace("\"", "\"\"");
        if (e.contains(';') || e.contains(',') || e.contains('\n'))
            return "\"" + e + "\"";
        return e;
    };
    QString out;
    out += "\xEF\xBB\xBF";
    out += tr("Bezeichnung")+";"+tr("Strasse")+";"+tr("Hausnr")+";"+
           tr("PLZ")+";"+tr("Ort")+";"+tr("Land")+"\n";
    for (const Adresse &a : adressen) {
        out += esc(a.bezeichnung)+";"+esc(a.strasse)+";"+esc(a.hausnummer)+";"+
               esc(a.plz)+";"+esc(a.ort)+";"+esc(a.land)+"\n";
    }
    return out;
}

QString ExportBridge::buildCsvZusammenfassung(const QVector<FahrtGruppe> &gruppen)
{
    auto esc = [](const QString &s) {
        return "\"" + QString(s).replace("\"", "\"\"") + "\"";
    };
    QString out;
    out += "\xEF\xBB\xBF";
    out += tr("Startadresse")+";"+tr("Zieladresse")+";"+
           tr("Anzahl Fahrten")+";"+tr("Gesamt (km)")+"\n";
    double total = 0;
    for (const FahrtGruppe &g : gruppen) {
        out += esc(g.startAdresse)+";"+esc(g.zielAdresse)+";"+
               QString::number(g.anzahl)+";"+
               QString::number(g.kmGesamt, 'f', 1)+"\n";
        total += g.kmGesamt;
    }
    out += ";;"+tr("Gesamt")+";"+QString::number(total, 'f', 1)+"\n";
    return out;
}

QString ExportBridge::buildHtmlFahrtenTabelle(const QVector<Fahrt> &fahrten, double &totalKm)
{
    totalKm = 0;
    bool mitFahrer = m_db->getEinstellungen().mehrerefahrer;
    int colSpan = mitFahrer ? 4 : 3;
    QString html;
    html += "<table border='1' cellpadding='4' cellspacing='0' "
            "style='font-family:Arial;font-size:10pt;width:100%;border-collapse:collapse;'>"
            "<tr style='background-color:#2E75B6;color:white;font-weight:bold;'>"
            "<th>"+tr("Datum")+"</th>";
    if (mitFahrer) html += "<th>"+tr("Fahrer")+"</th>";
    html += "<th>"+tr("Startadresse")+"</th><th>"+tr("Zieladresse")+"</th>"
            "<th>"+tr("Entfernung (km)")+"</th><th>"+tr("Hin & Zurück")+"</th><th>"+tr("Bemerkung")+"</th>"
            "</tr>";
    bool alt = false;
    for (const Fahrt &f : fahrten) {
        totalKm += f.entfernung;
        QString bg = alt ? "#f0f0f0" : "#ffffff"; alt = !alt;
        html += QString("<tr style='background-color:%1;'><td>%2</td>")
                .arg(bg, f.datum.toString("dd.MM.yyyy"));
        if (mitFahrer)
            html += QString("<td>%1</td>").arg(f.fahrerName.toHtmlEscaped());
        html += QString("<td>%1</td><td>%2</td>"
                        "<td align='right'>%3</td><td align='center'>%4</td><td>%5</td></tr>")
                .arg(f.startAdresse.toHtmlEscaped(), f.zielAdresse.toHtmlEscaped())
                .arg(QString::number(f.entfernung, 'f', 1))
                .arg(f.hinUndZurueck ? tr("Ja") : tr("Nein"))
                .arg(f.bemerkung.toHtmlEscaped());
    }
    html += QString("<tr style='font-weight:bold;background-color:#BDD7EE;'>"
                    "<td colspan='%1' align='right'><b>"+tr("Gesamt")+":</b></td>"
                    "<td align='right'><b>%2 km</b></td><td colspan='2'></td></tr>")
            .arg(colSpan).arg(QString::number(totalKm, 'f', 1));
    html += "</table>";
    return html;
}

QString ExportBridge::buildHtmlZusammenfassung(const QVector<FahrtGruppe> &gruppen)
{
    QString html;
    html += "<h3 style='font-family:Arial;margin-top:20px;'>"+tr("Zusammenfassung nach Strecke")+"</h3>";
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
                .arg(bg, g.startAdresse.toHtmlEscaped(), g.zielAdresse.toHtmlEscaped())
                .arg(g.anzahl).arg(QString::number(g.kmGesamt, 'f', 1));
    }
    html += QString("<tr style='font-weight:bold;background-color:#BDD7EE;'>"
                    "<td colspan='3' align='right'><b>"+tr("Gesamt")+":</b></td>"
                    "<td align='right'><b>%1 km</b></td></tr>")
            .arg(QString::number(total, 'f', 1));
    html += "</table>";
    return html;
}

