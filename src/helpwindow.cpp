#include "helpwindow.h"
#include <QVBoxLayout>
#include <QPushButton>
#include <QTextBrowser>
#include <QListWidget>
#include <QHBoxLayout>
#include <QFrame>
#include <QStringList>
#if defined(Q_OS_ANDROID)
#  include <QQuickWidget>
#  include <QQuickItem>
#  include <QQmlContext>
#endif

HelpWindow::HelpWindow(QWidget *parent)
    : QDialog(parent
#if defined(Q_OS_ANDROID)
              , Qt::Window
#endif
             )
{
    setWindowTitle(tr("Benutzeranleitung"));

#if defined(Q_OS_ANDROID)
    const QString appVer = QStringLiteral(APP_VERSION);
    const QString qtVer  = QStringLiteral(QT_VERSION_STR);

    QVariantList sections;
    auto addSection = [&](const QString &title, const QString &body) {
        QVariantMap m;
        m[QStringLiteral("title")] = title;
        m[QStringLiteral("body")]  = body;
        sections.append(m);
    };

    addSection(tr("Erste Schritte"),
        tr("1. Einstellungen öffnen → Adressen anlegen.\n"
           "2. Standardadresse und ggf. Standard-Fahrer festlegen.\n"
           "3. Fahrten erfassen über den + -Button in der Fahrtenliste."));

    addSection(tr("Navigation"),
        tr("Über das Burger-Menü oben links erreichst du:\n"
           "Fahrten · Adressen · Fahrer · Einstellungen\n"
           "Hilfe · Info · Beenden"));

    addSection(tr("Fahrten"),
        tr("+ legt eine neue Fahrt an.\n"
           "Tippe auf eine Fahrt zum Bearbeiten oder Löschen.\n"
           "Start- und Zieladresse wählen, Entfernung manuell eingeben\n"
           "oder per Berechnen-Button ermitteln lassen.\n"
           "Hin & Zurück verdoppelt die Entfernung automatisch."));

    addSection(tr("Adressen"),
        tr("+ legt eine neue Adresse an.\n"
           "Tippe auf eine Adresse zum Bearbeiten.\n"
           "Ort ist Pflichtfeld.\n"
           "Verwendete Adressen können nicht gelöscht werden."));

    addSection(tr("Fahrer"),
        tr("Sichtbar wenn 'Mehrere Fahrer' aktiv ist.\n"
           "Name ist Pflichtfeld.\n"
           "Fahrer 'unbekannt' wird automatisch angelegt und kann nicht gel\u00f6scht werden."));

    addSection(tr("Einstellungen"),
        tr("Datumsfilter: Monat/Jahr zum Einschränken der Anzeige.\n"
           "Standard-Werte: Vorbesetzung beim Anlegen neuer Fahrten.\n"
           "Mehrere Fahrer: aktiviert Fahrerverwaltung.\n"
           "Sprache: App-Sprache (Neustart erforderlich).\n"
#if !defined(Q_OS_ANDROID)
           "ORS API-Key: optional – für präzise Fahrtstrecke via openrouteservice.org.\n"
           "Ohne Key: automatisch OSRM (kostenlos, kein Account nötig).\n"
#endif
           "Synchronisation: WLAN-Direktsync mit zweitem Gerät.\n"
           "Abbrechen verwirft alle Änderungen, Speichern übernimmt sie."));

    addSection(tr("Distanzberechnung"),
#if defined(Q_OS_ANDROID)
        tr("Distanzberechnung erfolgt automatisch über OSRM (kostenlos, kein Account nötig).\n"
           "Ohne Netzwerk: Luftlinie × 1,3 als Näherungswert."));
#else
        tr("Ohne ORS API-Key: OSRM (kostenlos, kein Account, automatisch).\n"
           "Mit ORS API-Key: OpenRouteService – schnellste Fahrtstrecke, 500 Anfragen/Tag.\n"
           "Ohne Netzwerk: Luftlinie × 1,3 als Näherungswert.\n"
           "ORS-Key kostenlos unter openrouteservice.org erhältlich."));
#endif

    addSection(tr("Synchronisation (WLAN)"),
        tr("Beide Geräte müssen im selben WLAN sein.\n"
           "Einstellungen → Synchronisation → WLAN aktivieren.\n"
           "Sync startet automatisch beim Speichern."));

    addSection(tr("Technische Infos"),
        tr("Version: ") + appVer + tr("\nQt: ") + qtVer +
        tr("\nDatenbank: SQLite 3 (WAL-Modus)\nPlattformen: Windows 10/11, Android API 24+"));

    // Qt 6.8 / Android 16: QQuickWidget statt QQuickView.
    // QQuickWidget teilt den EGL-Context aller anderen QQuickWidgets → kein Konflikt.
    auto *pw = qobject_cast<QWidget*>(parent);
    auto *qw = new QQuickWidget(pw);
    qw->setAttribute(Qt::WA_DeleteOnClose);
    qw->setResizeMode(QQuickWidget::SizeRootObjectToView);
    qw->rootContext()->setContextProperty(QStringLiteral("helpSections"), sections);

    QObject::connect(qw, &QQuickWidget::statusChanged, qw,
        [qw](QQuickWidget::Status s) {
            if (s == QQuickWidget::Error)
                for (const auto &e : qw->errors())
                    qWarning() << "HelpView QML:" << e.toString();
            if (s != QQuickWidget::Ready) return;
            if (auto *root = qw->rootObject())
                QObject::connect(root, SIGNAL(closeRequested()),
                                 qw, SLOT(deleteLater()));
        });

    qw->setSource(QUrl(QStringLiteral("qrc:/HelpView.qml")));
    if (pw) qw->setGeometry(0, 0, pw->width(), pw->height());
    qw->show();
    qw->raise();
    hide();

#else
    // ── Desktop: Sidebar + QTextBrowser ──────────────────────────────────────
    setWindowFlags(Qt::Window);
    resize(1060, 780);

    const QString appVer = QStringLiteral(APP_VERSION);
    const QString qtVer  = QStringLiteral(QT_VERSION_STR);

    auto *hLay = new QHBoxLayout(this);
    hLay->setContentsMargins(0, 0, 0, 0);
    hLay->setSpacing(0);

    auto *navList = new QListWidget(this);
    navList->setFixedWidth(148);
    navList->setFrameShape(QFrame::NoFrame);
    navList->setStyleSheet(
        "QListWidget { background: #FFFFFF; border-right: 1px solid #DCE4E9; font-size: 11pt; }"
        "QListWidget::item { padding: 7px 14px; color: #1A1C1E; }"
        "QListWidget::item:selected { background: #EDF4F8; color: #006493; font-weight: bold;"
        "                             border-left: 3px solid #006493; }"
        "QListWidget::item:hover:!selected { background: #F2F4F5; }");
    navList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    const QStringList navLabels = {
        tr("Kurzbeschreibung"),
        tr("Erste Schritte"), tr("Fahrten"), tr("Adressen"), tr("Fahrer"),
        tr("Einstellungen"), tr("Datenexport"), tr("Distanzberechnung"),
        tr("Synchronisation"), tr("Datensicherung"), tr("Technik") };
    const QStringList navAnchors = {
        "kurz", "start", "fahrten", "adressen", "fahrer",
        "einstellungen", "export", "distanz", "sync", "sicherung", "technik" };
    for (const QString &lbl : navLabels)
        navList->addItem(lbl);
    navList->setCurrentRow(0);
    hLay->addWidget(navList);

    auto *browser = new QTextBrowser(this);
    browser->setOpenExternalLinks(true);

    // ── HTML zusammenbauen aus übersetzten Einzelteilen ───────────────────────
    const QString css =
        "<html><head><style>"
        "body  { font-family: Segoe UI, Arial, sans-serif; font-size: 11pt;"
        "        line-height: 1.7; margin: 20px 28px; color: #1A1C1E; background: #F8FAFB; }"
        "h1 { font-size: 18pt; color: #006493; border-bottom: 2px solid #006493;"
        "     padding-bottom: 8px; margin-top: 0; }"
        "h2 { font-size: 13pt; color: #006493; margin-top: 28px; margin-bottom: 6px;"
        "     border-bottom: 1px solid #DCE4E9; padding-bottom: 4px; }"
        "h3 { font-size: 11pt; margin-top: 14px; margin-bottom: 2px; }"
        "p  { margin: 5px 0; }"
        "ul,ol { margin: 4px 0 4px 20px; } li { margin-bottom: 2px; }"
        "table.data { border-collapse: collapse; width: 100%; margin: 8px 0; font-size: 10.5pt; }"
        "table.data th { background: #006493; color: white; padding: 6px 10px; text-align: left; }"
        "table.data td { border: 1px solid #DCE4E9; padding: 5px 9px; vertical-align: top; }"
        "table.data tr:nth-child(even) td { background: #F2F4F5; }"
        "code { background: #F2F4F5; border: 1px solid #DCE4E9; border-radius: 3px;"
        "       padding: 1px 4px; font-family: Consolas, monospace; font-size: 10pt; }"
        ".hint { background: #EDF4F8; border-left: 4px solid #006493; padding: 9px 13px; margin: 10px 0; }"
        ".warn { background: #FFF8E1; border-left: 4px solid #F0A500; padding: 9px 13px; margin: 10px 0; }"
        ".ok   { background: #E8F5E9; border-left: 4px solid #43A047; padding: 9px 13px; margin: 10px 0; }"
        ".ver  { color: #72787E; font-size: 10pt; }"
        "</style></head><body>";

    const QString html = css
        + "<h1>Fahrtenbuch – " + tr("Benutzeranleitung") + "</h1>"
        + "<p class='ver'>" + tr("Version") + " " + appVer
        + " &nbsp;|&nbsp; Qt " + qtVer + " &nbsp;|&nbsp; Windows 10/11</p>"

        // Kurzbeschreibung
        + "<h2><a name='kurz'></a>" + tr("Kurzbeschreibung") + "</h2>"
        + "<p><b>Fahrtenbuch</b> " + tr("ist eine Anwendung zur digitalen Erfassung und Verwaltung von Dienstfahrten "
          "unter Windows und Android. Daten werden lokal in einer SQLite-Datenbank gespeichert und "
          "k&ouml;nnen zwischen PC und Mobilger&auml;t per WLAN synchronisiert werden.") + "</p>"
        + "<ul>"
        + "<li><b>" + tr("Fahrtenerfassung") + "</b> – " + tr("Datum, Start- und Zieladresse, Entfernung, Fahrer, Bemerkung") + "</li>"
        + "<li><b>" + tr("Entfernungsberechnung") + "</b> – "
#if defined(Q_OS_ANDROID)
        + tr("automatisch über OSRM (kostenlos, kein Account nötig)")
#else
        + tr("OSRM (kostenlos, kein Account) oder OpenRouteService mit API-Key (500/Tag)")
#endif
        + "</li>"
        + "<li><b>" + tr("Adressverwaltung") + "</b> – " + tr("Adressbuch mit CSV-Import") + "</li>"
        + "<li><b>" + tr("Fahrerverwaltung") + "</b> – " + tr("optionaler Mehrfahrer-Modus") + "</li>"
        + "<li><b>" + tr("Datenexport") + "</b> – " + tr("PDF (A4-Querformat) und CSV (Excel-kompatibel)") + "</li>"
        + "<li><b>" + tr("WLAN-Synchronisation") + "</b> – " + tr("automatischer Abgleich zwischen Windows-PC und Android im Heimnetz") + "</li>"
        + "</ul>"

        // Erste Schritte
        + "<h2><a name='start'></a>" + tr("Erste Schritte") + "</h2>"
        + "<ol>"
        + "<li><b>" + tr("Einstellungen") + "</b> " + tr("aufrufen → Standardadresse und Standardfahrer festlegen.") + "</li>"
        + "<li>" + tr("Tab") + " <b>" + tr("Adressen") + "</b>: " + tr("häufig genutzte Adressen anlegen.") + "</li>"
        + "<li>" + tr("Optional Tab") + " <b>" + tr("Fahrer") + "</b> (" + tr("sichtbar wenn") + " <i>" + tr("Mehrere Fahrer") + "</i> " + tr("aktiviert") + ").</li>"
        + "<li>" + tr("Tab") + " <b>" + tr("Fahrten") + "</b>: " + tr("erste Fahrt über") + " <b>+ " + tr("Neue Fahrt") + "</b> " + tr("erfassen.") + "</li>"
        + "<li>" + tr("Entfernungen werden automatisch per OSRM berechnet (kein Account n\u00f6tig)."
#if !defined(Q_OS_ANDROID)
            " Optional: ORS API-Key in Einstellungen f\u00fcr pr\u00e4zisere Strecken."
#endif
            ) + "</li>"
        + "</ol>"

        // Fahrten
        + "<h2><a name='fahrten'></a>" + tr("Fahrten") + "</h2>"
        + "<h3>" + tr("Tabelle") + "</h3>"
        + "<p>" + tr("Zeigt alle Fahrten des eingestellten Zeitraums. Klick auf eine Spaltenüberschrift sortiert; zweiter Klick kehrt um.") + "</p>"
        + "<h3>" + tr("Neue Fahrt / Bearbeiten") + "</h3>"
        + "<p><b>+ " + tr("Neue Fahrt") + "</b>. <b>" + tr("Doppelklick") + "</b> " + tr("öffnet den Bearbeitungsdialog.") + "</p>"
        + "<h3>" + tr("Felder im Eingabedialog") + "</h3>"
        + "<table class='data'><tr><th>" + tr("Feld") + "</th><th>" + tr("Beschreibung") + "</th></tr>"
        + "<tr><td>" + tr("Datum") + "</td><td>" + tr("Vorbesetzt mit dem aktuellen Datum.") + "</td></tr>"
        + "<tr><td>" + tr("Fahrer") + "</td><td>" + tr("Dropdown; nur im Mehrfahrer-Modus sichtbar.") + "</td></tr>"
        + "<tr><td>" + tr("Startadresse") + " *</td><td>" + tr("Pflichtfeld.") + "</td></tr>"
        + "<tr><td>" + tr("Zieladresse") + " *</td><td>" + tr("Pflichtfeld.") + "</td></tr>"
        + "<tr><td>" + tr("Entfernung (km)") + "</td><td>" + tr("Manuell oder über Berechnen automatisch ermitteln.") + "</td></tr>"
        + "<tr><td>" + tr("Hin & Zurück") + "</td><td>" + tr("Verdoppelt die Entfernung automatisch.") + "</td></tr>"
        + "<tr><td>" + tr("Bemerkung") + "</td><td>" + tr("Optionaler Freitext.") + "</td></tr>"
        + "</table>"
        + "<div class='ok'><b>" + tr("Speichern") + "</b> " + tr("schreibt die Fahrt und lädt sie bei aktiver Synchronisation sofort hoch.") + "</div>"

        // Adressen
        + "<h2><a name='adressen'></a>" + tr("Adressen") + "</h2>"
        + "<table class='data'><tr><th>" + tr("Aktion") + "</th><th>" + tr("Beschreibung") + "</th></tr>"
        + "<tr><td>+ " + tr("Neue Adresse") + "</td><td>" + tr("Eingabedialog öffnen") + "</td></tr>"
        + "<tr><td>" + tr("Doppelklick") + "</td><td>" + tr("Adresse bearbeiten") + "</td></tr>"
        + "<tr><td>" + tr("Löschen") + "</td><td>" + tr("Nur möglich wenn Adresse in keiner Fahrt verwendet wird") + "</td></tr>"
        + "<tr><td>" + tr("CSV importieren") + "</td><td>" + tr("Semikolon-getrennt; Spalten: Bezeichnung; Straße; Hausnr.; PLZ; Ort") + "</td></tr>"
        + "</table>"

        // Fahrer
        + "<h2><a name='fahrer'></a>" + tr("Fahrer") + "</h2>"
        + "<p>" + tr("Sichtbar wenn") + " <b>" + tr("Mehrere Fahrer") + "</b> " + tr("aktiviert.")
        + " " + tr("Felder:") + " <b>" + tr("Name") + "</b> (" + tr("Pflicht") + ").</p>"

        // Einstellungen
        + "<h2><a name='einstellungen'></a>" + tr("Einstellungen") + "</h2>"
        + "<table class='data'><tr><th>" + tr("Einstellung") + "</th><th>" + tr("Beschreibung") + "</th></tr>"
        + "<tr><td>" + tr("Datenbank-Pfad") + "</td><td>" + tr("Standard:") + " <code>%APPDATA%/Fahrtenbuch/fahrtenbuch.db</code></td></tr>"
        + "<tr><td>" + tr("Datumsfilter") + "</td><td>" + tr("Monat/Jahr für Anzeige und Export. 0 = kein Filter.") + "</td></tr>"
        + "<tr><td>" + tr("Standardadresse") + "</td><td>" + tr("Vorbesetzung der Startadresse bei neuen Fahrten.") + "</td></tr>"
        + "<tr><td>" + tr("Standardfahrer") + "</td><td>" + tr("Vorbesetzung des Fahrers (nur Mehrfahrer-Modus).") + "</td></tr>"
        + "<tr><td>" + tr("Hin & Zurück") + "</td><td>" + tr("Standardwert der Checkbox bei neuen Fahrten.") + "</td></tr>"
        + "<tr><td>" + tr("Mehrere Fahrer") + "</td><td>" + tr("Schaltet Fahrerverwaltung ein.") + "</td></tr>"
        + "<tr><td>" + tr("Sprache") + "</td><td>" + tr("App-Sprache; Neustart erforderlich.") + "</td></tr>"
        + "<tr><td>" + tr("Synchronisation") + "</td><td>" + tr("Modus: Deaktiviert / WLAN.") + "</td></tr>"
        + "</table>"
        + "<p><b>" + tr("Abbrechen") + "</b> " + tr("verwirft alle Änderungen.") + " <b>" + tr("Speichern") + "</b> " + tr("übernimmt sie.") + "</p>"

        // Datenexport
        + "<h2><a name='export'></a>" + tr("Datenexport") + "</h2>"
        + "<p><b>CSV</b>: " + tr("Semikolon-getrennt, UTF-8, direkt in Excel öffenbar.") + "<br>"
        + "<b>PDF</b>: " + tr("A4-Querformat mit Tabelle und Kilometergesamtsumme.") + "</p>"

        // Distanzberechnung
        + "<h2><a name='distanz'></a>" + tr("Distanzberechnung") + "</h2>"
        + "<ol><li><b>" + tr("Geocoding") + "</b> (Nominatim): " + tr("Adressen → GPS-Koordinaten.") + "</li>"
#if defined(Q_OS_ANDROID)
        + "<li><b>" + tr("Routing") + "</b>: " + tr("OSRM (kostenlos, kein Account nötig).") + "</li></ol>"
        + "<div class='hint'>" + tr("Die Distanzberechnung erfolgt automatisch über OSRM – kostenlos, kein Account erforderlich.") + "</div>"
#else
        + "<li><b>" + tr("Routing") + "</b>: " + tr("Ohne Key: OSRM (kostenlos, kein Account). Mit ORS-Key: schnellste Strecke.") + "</li></ol>"
        + "<div class='hint'>" + tr("Die App verwendet standardmäßig OSRM – kostenlos, kein Account nötig. "
            "Für präzisere Streckenberechnung kann ein kostenloser ORS API-Key "
            "(openrouteservice.org, 500 Anfragen/Tag) in den Einstellungen hinterlegt werden.") + "</div>"
#endif

        // Synchronisation
        + "<h2><a name='sync'></a>" + tr("Synchronisation (WLAN)") + "</h2>"
        + "<p>" + tr("Direkter Datenbankabgleich zwischen Windows-PC und Android im selben Heimnetz – ohne Cloud, ohne Internet.") + "</p>"
        + "<h3>" + tr("Einrichtung") + "</h3>"
        + "<ol>"
        + "<li>" + tr("Einstellungen → Synchronisation → Modus: WLAN (automatisch) → Speichern.") + "</li>"
        + "<li>" + tr("Beide Geräte müssen im selben WLAN sein.") + "</li>"
        + "</ol>"
        + "<table class='data'><tr><th>" + tr("Zeitpunkt") + "</th><th>" + tr("Was passiert") + "</th></tr>"
        + "<tr><td>" + tr("Beim Speichern") + "</td><td>" + tr("Daten werden sofort auf das andere Gerät übertragen.") + "</td></tr>"
        + "<tr><td>" + tr("Offline / PC aus") + "</td><td>" + tr("Lokal gespeichert; beim nächsten Speichern erneuter Versuch.") + "</td></tr>"
        + "</table>"

        // Datensicherung
        + "<h2><a name='sicherung'></a>" + tr("Datensicherung") + "</h2>"
        + "<p>" + tr("Standard:") + " <code>%APPDATA%/Fahrtenbuch/fahrtenbuch.db</code><br>"
        + tr("Manuelle Sicherung: diese Datei kopieren.") + "</p>"

        // Technik
        + "<h2><a name='technik'></a>" + tr("Technische Informationen") + "</h2>"
        + "<table class='data'><tr><th>" + tr("Komponente") + "</th><th>" + tr("Details") + "</th></tr>"
        + "<tr><td>" + tr("Version") + "</td><td>" + appVer + "</td></tr>"
        + "<tr><td>Qt</td><td>" + qtVer + "</td></tr>"
        + "<tr><td>" + tr("Datenbank") + "</td><td>SQLite 3 WAL</td></tr>"
        + "<tr><td>" + tr("Plattformen") + "</td><td>Windows 10/11, Android API 24+</td></tr>"
        + "<tr><td>Geocoding</td><td>Nominatim (OpenStreetMap)</td></tr>"
        + "<tr><td>Routing</td><td>"
#if defined(Q_OS_ANDROID)
        + "OSRM (kostenlos, kein Account)"
#else
        + "OSRM (Standard, kostenlos) / OpenRouteService (optional)"
#endif
        + "</td></tr>"
        + "<tr><td>" + tr("Einstellungen") + "</td><td>" + tr("Windows-Registry") + "</td></tr>"
        + "</table>"
        + "</body></html>";

    browser->setHtml(html);
    hLay->addWidget(browser, 1);

    QObject::connect(navList, &QListWidget::currentRowChanged, browser,
        [browser, navAnchors](int row) {
            if (row >= 0 && row < navAnchors.size())
                browser->scrollToAnchor(navAnchors[row]);
        });

    auto *wrapper = new QWidget(this);
    auto *vLay    = new QVBoxLayout(wrapper);
    vLay->setContentsMargins(0, 0, 0, 0);
    vLay->setSpacing(0);

    auto *mainArea = new QWidget(wrapper);
    mainArea->setLayout(hLay);
    vLay->addWidget(mainArea, 1);

    auto *closeBtn = new QPushButton(tr("Schließen"), wrapper);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    auto *btnRow = new QHBoxLayout();
    btnRow->setContentsMargins(8, 4, 8, 6);
    btnRow->addStretch();
    btnRow->addWidget(closeBtn);
    vLay->addLayout(btnRow);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(wrapper);
#endif
}
