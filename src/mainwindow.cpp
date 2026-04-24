// mainwindow.cpp

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "aboutdialog.h"
#include "helpwindow.h"
#include "settings.h"
#include "snackbar.h"
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QCloseEvent>
#if defined(Q_OS_ANDROID)
#  include <csignal>
#  include <unistd.h>
#endif
#include <QMessageBox>
#include <QApplication>
#include <QGuiApplication>
#include <QStyleFactory>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QTimer>
#include <QEvent>
#include <QResizeEvent>
#include <QKeyEvent>
#include <QFileInfo>
#include <QSettings>

#if defined(Q_OS_WIN)
#include <QHeaderView>
// ── Fenster- und Spaltenbreiten in Registry speichern / laden ─────────────────
static void saveHeaderState(const QString &key, QHeaderView *hv)
{
    if (!hv) return;
    QSettings s;
    s.setValue(key, hv->saveState());
}

static void restoreHeaderState(const QString &key, QHeaderView *hv,
                               const QList<int> &defaults)
{
    if (!hv) return;
    QSettings s;
    QByteArray state = s.value(key).toByteArray();
    if (!state.isEmpty()) {
        hv->restoreState(state);
    } else {
        // Standard-Breiten setzen wenn noch keine gespeicherten Werte
        for (int i = 0; i < defaults.size(); ++i)
            if (defaults[i] > 0) hv->resizeSection(i, defaults[i]);
    }
}
#endif // Q_OS_WIN
#include <QFileDialog>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include "swipefilter.h"
#if defined(Q_OS_ANDROID)
#  include <QQuickWidget>
#  include <QQmlContext>
#  include <QQuickItem>
#  include "exportbridge.h"
#endif
#include <QToolButton>
#include <QHBoxLayout>
#include <QFrame>
#include <QProgressBar>
#include <QLineEdit>
#include <QHostAddress>
#include <QSqlQuery>
#if defined(Q_OS_ANDROID)
#  include <QJniObject>
#  include <QJniEnvironment>
#endif

#if defined(Q_OS_ANDROID)
// ─── AppBarBridge ─────────────────────────────────────────────────────────────
class AppBarBridge : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString pageTitle   READ pageTitle   WRITE setPageTitle  NOTIFY pageTitleChanged)
    Q_PROPERTY(QString syncStatus  READ syncStatus  WRITE setSyncStatus NOTIFY syncStatusChanged)
    Q_PROPERTY(QString appVersion  READ appVersion  CONSTANT)
public:
    explicit AppBarBridge(QObject *parent = nullptr) : QObject(parent) {}
    QString pageTitle()  const { return m_pageTitle;  }
    QString syncStatus() const { return m_syncStatus; }
    QString appVersion()  const { return QStringLiteral(APP_VERSION); }
    void setPageTitle (const QString &v) { if (m_pageTitle  != v) { m_pageTitle  = v; emit pageTitleChanged();  } }
    void setSyncStatus(const QString &v) { if (m_syncStatus != v) { m_syncStatus = v; emit syncStatusChanged(); } }

    // Versteckter Sync-Log-Aktivator: 7 Taps auf die Versionsnummer
    Q_INVOKABLE void onVersionTapped() {
        if (m_syncLogUnlocked) return;  // bereits freigeschaltet
        if (++m_tapCount >= 7) {
            m_syncLogUnlocked = true;
            m_tapCount = 0;
            emit syncLogActivated();
        }
    }
    bool syncLogUnlocked() const { return m_syncLogUnlocked; }

signals:
    void pageTitleChanged();
    void syncStatusChanged();
    void syncLogActivated();  // wird genau einmal pro Session emittiert

private:
    QString m_pageTitle;  // wird nach Translator-Installation gesetzt
    QString m_syncStatus;
    int     m_tapCount        = 0;
    bool    m_syncLogUnlocked = false;
};


#endif // bridge classes


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle(tr("Fahrtenbuch") + QString(" v%1").arg(APP_VERSION));
#if defined(Q_OS_WIN)
    setMinimumSize(900, 600);
    resize(1200, 750);
#endif

    // ── Datenbank ────────────────────────────────────────────────────────────
    m_db = new Database(this);
    if (!m_db->open(Database::defaultDbPath())) {
        QMessageBox::critical(this, tr("Datenbankfehler"),
            tr("Die Datenbank konnte nicht geoeffnet werden:\n%1").arg(m_db->lastError()));
    }

    // ── Services ─────────────────────────────────────────────────────────────
    m_distSvc = new DistanceService(this);

    connect(m_distSvc, &DistanceService::coordsResolved,
            this, [this](int adresseId, double lat, double lon) {
        m_db->updateAdresseCoords(adresseId, lat, lon);
    });

    // ORS-Key ungültig → einmalige Warnung, danach Haversine-Fallback
    connect(m_distSvc, &DistanceService::apiKeyInvalid, this, [this] {
        static bool shown = false;
        if (!shown) {
            shown = true;
            Snackbar::show(this, tr("ORS-API-Key ungueltig oder Limit erreicht – "
                                    "Entfernung wird geschaetzt (Luftlinie x 1.3)"), 6000);
        }
    });

    m_syncMgr = new SyncManager(m_db, this);

    Einstellungen e = m_db->getEinstellungen();
    m_lastKnownLanguage = e.language;  // Basiswert für Sprach-Änderungs-Vergleich

    // Sprache aus DB → QSettings übertragen, damit main.cpp sie beim nächsten
    // Start findet (main.cpp läuft vor DB-Initialisierung).
    {
        QSettings s;
        if (s.value(QStringLiteral("app/language")).toString() != e.language)
            s.setValue(QStringLiteral("app/language"), e.language);
    }
    m_distSvc->setApiKey(e.apiKeyDistance);

    // Sync-DB immer initialisieren – unabhängig vom syncMode.
    // Legt Trigger + sync_log an und migriert bestehende Daten einmalig.
    m_syncMgr->initDb();

    m_syncMgr->applySettings(e);

    // AppBar-Status nach Start setzen (wird später von syncStarted/syncFinished aktualisiert)
#if defined(Q_OS_ANDROID)
    if (m_appBarBridge) m_appBarBridge->setSyncStatus(
        m_syncMgr->isRunning() ? tr("Sync: WLAN") : tr("Sync: aus"));
#endif

    // ── Tabs ─────────────────────────────────────────────────────────────────
    m_tabs = ui->tabWidget;

#if defined(Q_OS_ANDROID)
    m_tabs->setAttribute(Qt::WA_AcceptTouchEvents);
    m_swipeFilter = new SwipeFilter(m_tabs, this);
#endif

    m_fahrtenTab = new FahrtenTable(m_db, m_distSvc, m_syncMgr, this);
    ui->layoutFahrten->addWidget(m_fahrtenTab);

    m_adressTab = new AdressTable(m_db, m_syncMgr, this);
    ui->layoutAdressen->addWidget(m_adressTab);

    m_fahrerTab = new FahrerTable(m_db, m_syncMgr, this);
    ui->layoutFahrer->addWidget(m_fahrerTab);

    m_einstellTab = new EinstellungenWidget(m_db, m_syncMgr, this);
    ui->layoutEinstellungen->addWidget(m_einstellTab);

    m_exportTab = new DatenExport(m_db, this);
    ui->layoutExport->addWidget(m_exportTab);

#if defined(Q_OS_ANDROID)
    {
        int idx = m_tabs->indexOf(ui->tabExport);
        if (idx != -1) m_tabs->removeTab(idx);
    }
#endif

    // ── Initialer Datenlade ───────────────────────────────────────────────────
    m_initialLoadDone = true;
    m_fahrtenTab->refresh();
    m_adressTab->refresh();
    m_fahrerTab->refresh();

    m_einstellTab->load();
    m_exportTab->refresh();

    {
        Einstellungen startE = m_db->getEinstellungen();
        m_fahrtenTab->setFahrerVisible(startE.mehrerefahrer);
        if (!startE.mehrerefahrer) {
            int idx = m_tabs->indexOf(ui->tabFahrer);
            if (idx != -1) m_tabs->removeTab(idx);
        }
        ui->actionFahrer->setVisible(startE.mehrerefahrer);
    }

    setupStatusBar();

    // Sync-Menü immer anzeigen
    ui->menuSync->menuAction()->setVisible(true);

    // ── Actions verdrahten ────────────────────────────────────────────────────
    connect(ui->actionSpeichern, SIGNAL(triggered()), this, SLOT(onSaveAll()));
    connect(ui->actionBeenden,   SIGNAL(triggered()), this, SLOT(close()));

    connect(ui->actionFahrten,       SIGNAL(triggered()), this, SLOT(onNavFahrten()));
    connect(ui->actionAdressen,      SIGNAL(triggered()), this, SLOT(onNavAdressen()));
    connect(ui->actionFahrer,        SIGNAL(triggered()), this, SLOT(onNavFahrer()));
    connect(ui->actionEinstellungen, SIGNAL(triggered()), this, SLOT(onNavEinstellungen()));
    connect(ui->actionExport,        SIGNAL(triggered()), this, SLOT(onNavExport()));

    connect(ui->actionSyncStarten,   SIGNAL(triggered()), this, SLOT(onSyncStart()));
    connect(ui->actionSyncStatus,    SIGNAL(triggered()), this, SLOT(onSyncStatus()));
    connect(ui->actionSyncProtokoll, SIGNAL(triggered()), this, SLOT(showSyncLog()));

    connect(ui->actionBenutzeranleitung,  SIGNAL(triggered()), this, SLOT(showHelp()));
    connect(ui->actionUeberQt,            SIGNAL(triggered()), this, SLOT(showQtAbout()));
    connect(ui->actionUeberFahrtenbuch,   SIGNAL(triggered()), this, SLOT(showAbout()));

    connect(m_tabs, SIGNAL(currentChanged(int)), this, SLOT(onTabChanged(int)));

    connect(m_fahrtenTab, SIGNAL(dataChanged()), this, SLOT(onDataChanged()));
    connect(m_adressTab,  SIGNAL(dataChanged()), this, SLOT(onDataChanged()));
    connect(m_fahrerTab,  SIGNAL(dataChanged()), this, SLOT(onDataChanged()));

#if defined(Q_OS_ANDROID)
    // "Gespeichert" nach 2 s wieder auf Sync-Status zurücksetzen
    auto showSaved = [this]() {
        if (!m_appBarBridge) return;
        m_appBarBridge->setSyncStatus(tr("✓ Gespeichert"));
        QTimer::singleShot(2000, this, [this]() {
            if (!m_appBarBridge) return;
            m_appBarBridge->setSyncStatus(
                m_syncMgr && m_syncMgr->isRunning() ? tr("Sync: WLAN") : tr("Sync: aus"));
        });
    };
    connect(m_fahrtenTab, &FahrtenTable::savedToDb, this, showSaved);
    connect(m_adressTab,  &AdressTable::savedToDb,  this, showSaved);
    connect(m_fahrerTab,  &FahrerTable::savedToDb,  this, showSaved);
    // Nach lokalem Speichern sofort Discovery starten damit Peers zeitnah sync bekommen
    connect(m_fahrtenTab, &FahrtenTable::savedToDb, m_syncMgr, &SyncManager::triggerDiscovery);
    connect(m_adressTab,  &AdressTable::savedToDb,  m_syncMgr, &SyncManager::triggerDiscovery);
    connect(m_fahrerTab,  &FahrerTable::savedToDb,  m_syncMgr, &SyncManager::triggerDiscovery);
#endif

    connect(m_einstellTab, SIGNAL(settingsChanged()), this, SLOT(onSettingsChanged()));
    connect(m_einstellTab, &EinstellungenWidget::cancelRequested,
            this, &MainWindow::onNavFahrten);

    connect(m_syncMgr, SIGNAL(syncFinished(bool,QString)), this, SLOT(onSyncFinished(bool,QString)));
    connect(m_syncMgr, SIGNAL(syncProgress(QString)),      this, SLOT(onSyncProgress(QString)));
    connect(m_syncMgr, &SyncManager::syncStarted,          this, &MainWindow::onSyncStatus);

#if defined(Q_OS_ANDROID)
    // VPN-Warnung als Snackbar anzeigen
    connect(m_syncMgr, &SyncManager::vpnWarning, this, [this]() {
        Snackbar::show(this,
            tr("⚠ VPN aktiv – WLAN-Sync möglicherweise eingeschränkt"),
            5000);
    });
#endif

    // changeApplied: sofort refreshen wenn eine eingehende Änderung angewendet wurde
    // (syncFinished wird nicht immer emittiert wenn S22 direkt sendet ohne SYNC_REQUEST)
    // Refresh via QTimer::singleShot um mehrfache schnelle Aufrufe zu bündeln
    if (m_syncMgr->dbSync()) {
        connect(m_syncMgr->dbSync(), &DatabaseSync::changeApplied,
                this, [this](const QString &, int) {
            // Verzögert refreshen – bündelt alle Änderungen eines Batches
            QTimer::singleShot(200, this, [this]() {
                m_fahrtenTab->refresh();
                m_adressTab->refresh();
                m_fahrerTab->refresh();
            });
        });
    }

    // Nach empfangener WLAN-Änderung UI aktualisieren
    connect(m_syncMgr, &SyncManager::syncFinished, this, [this](bool ok, const QString &) {
        if (ok) {
            m_fahrtenTab->refresh();
            m_adressTab->refresh();
            m_fahrerTab->refresh();
        }
    });

    // WLAN-Sync automatisch starten (nicht über onSyncStart – der ist ein Toggle)
#if defined(Q_OS_ANDROID)
    // Akku-Schonung: Sync pausieren wenn App in den Hintergrund geht
    connect(qApp, &QGuiApplication::applicationStateChanged,
            this, &MainWindow::onApplicationStateChanged);
#endif
    if (e.syncMode == "wifi") {
        QTimer::singleShot(2000, this, [this]() {
            if (!m_syncMgr->isRunning())
                m_syncMgr->startSync();
        });
    }

    applyTheme();

#if defined(Q_OS_WIN)
    // Fenstergeometrie und Spaltenbreiten aus Registry wiederherstellen
    {
        QSettings s;
        const QByteArray geo   = s.value(QStringLiteral("mainwindow/geometry")).toByteArray();
        const QByteArray state = s.value(QStringLiteral("mainwindow/state")).toByteArray();
        if (!geo.isEmpty())   restoreGeometry(geo);
        if (!state.isEmpty()) restoreState(state);

        restoreHeaderState(QStringLiteral("fahrten/header"),
            m_fahrtenTab->tableWidget()
                ? m_fahrtenTab->tableWidget()->horizontalHeader() : nullptr,
            {120, 150, 200, 200, 130, 90});

        restoreHeaderState(QStringLiteral("adressen/header"),
            m_adressTab->tableWidget()
                ? m_adressTab->tableWidget()->horizontalHeader() : nullptr,
            {180, 180, 70, 70, 0});

        restoreHeaderState(QStringLiteral("fahrer/header"),
            m_fahrerTab->tableWidget()
                ? m_fahrerTab->tableWidget()->horizontalHeader() : nullptr,
            {220, 180, 0});
    }
#endif

#if defined(Q_OS_ANDROID)
    setupAndroidNav();
    // Titel der AppBar nach Translator-Installation korrekt setzen
    onTabChanged(0);
    // NEARBY_WIFI_DEVICES anfordern (Android 13+ / API 33+) für WLAN-Sync Discovery.
    QTimer::singleShot(300, this, [this]() { checkAndRequestNearbyWifiPermission(); });
#endif
}

MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::setupStatusBar()
{
    m_statusLabel = new QLabel(tr("Bereit"), this);
    m_syncLabel   = new QLabel("", this);
    ui->statusBar->addWidget(m_statusLabel, 1);
    ui->statusBar->addPermanentWidget(m_syncLabel);
}

void MainWindow::applyTheme()
{
    qApp->setStyle(QStyleFactory::create("Fusion"));

    QPalette p;
    p.setColor(QPalette::Window,          QColor(245,247,250));
    p.setColor(QPalette::WindowText,      QColor( 30, 30, 40));
    p.setColor(QPalette::Base,            QColor(255,255,255));
    p.setColor(QPalette::AlternateBase,   QColor(235,241,250));
    p.setColor(QPalette::Button,          QColor(255,255,255));
    p.setColor(QPalette::ButtonText,      QColor( 30, 30, 40));
    p.setColor(QPalette::Highlight,       QColor( 46,117,182));
    p.setColor(QPalette::HighlightedText, QColor(255,255,255));
    qApp->setPalette(p);

#if defined(Q_OS_ANDROID)
    qApp->setStyleSheet(
        "QTabBar::tab { min-width: 90px; min-height: 48px; padding: 6px 12px;"
        "               font-size: 13pt; }"
        "QTabBar::tab:selected { background: #2E75B6; color: white; font-weight: bold; }"
        "QTabBar::tab:!selected { background: #e8edf4; color: #333; }"
        "QTabWidget::pane { border: none; }"
        "QPushButton { background: #2E75B6; color: white; border: none;"
        "              padding: 10px 20px; border-radius: 6px; font-size: 13pt;"
        "              min-height: 48px; }"
        "QPushButton:pressed { background: #155791; }"
        "QTableWidget { gridline-color: #dde; font-size: 12pt; }"
        "QTableWidget::item { padding: 6px 4px; min-height: 44px; }"
        "QHeaderView::section { background: #2E75B6; color: white; padding: 10px 8px;"
        "                        border: none; font-weight: bold; font-size: 12pt; }"
        "QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox, QDateEdit {"
        "    font-size: 13pt; min-height: 48px; padding: 4px 8px; }"
        "QComboBox::drop-down { width: 40px; }"
        "QComboBox::down-arrow { width: 20px; height: 20px; }"
        "QCheckBox { font-size: 13pt; spacing: 10px; min-height: 44px; }"
        "QCheckBox::indicator { width: 28px; height: 28px; }"
        "QLabel { font-size: 13pt; }"
        "QGroupBox { font-weight: bold; border: 1px solid #ccc; border-radius: 6px;"
        "            margin-top: 14px; padding-top: 8px; font-size: 13pt; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 6px; }"
        "QStatusBar { background: #1F4E79; color: white; font-size: 11pt; }"
        "QScrollBar:vertical { width: 0px; }"
        "QScrollBar:horizontal { height: 0px; }"
        "QDialogButtonBox QPushButton { min-width: 100px; min-height: 48px; }"
        "QMenuBar { max-height: 0px; padding: 0px; margin: 0px; }"
    );
    showMaximized();

#else
    qApp->setStyleSheet(
        "QMenuBar { background: #2E75B6; color: white; padding: 2px; }"
        "QMenuBar::item:selected { background: #1F4E79; }"
        "QMenu { background: white; border: 1px solid #ccc; }"
        "QMenu::item:selected { background: #BDD7EE; color: black; }"
        "QTabBar::tab { min-width: 120px; min-height: 28px; padding: 4px 16px; font-size: 11pt; }"
        "QTabBar::tab:selected { background: #2E75B6; color: white; font-weight: bold; }"
        "QTabBar::tab:!selected { background: #e8edf4; color: #333; }"
        "QPushButton { background: #2E75B6; color: white; border: none;"
        "              padding: 8px 16px; border-radius: 4px; font-size: 11pt; }"
        "QPushButton:hover { background: #1F4E79; }"
        "QPushButton:pressed { background: #155791; }"
        "QToolBar { background: #f0f4fa; border-bottom: 1px solid #ccd; padding: 4px; spacing: 6px; }"
        "QToolBar QPushButton { background: #2E75B6; color: white; border-radius: 4px;"
        "                       padding: 6px 14px; font-size: 11pt; }"
        "QToolBar QPushButton:hover { background: #1F4E79; }"
        "QGroupBox { font-weight: bold; border: 1px solid #ccc;"
        "            border-radius: 4px; margin-top: 10px; padding-top: 6px; font-size: 11pt; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }"
        "QTableWidget { gridline-color: #dde; font-size: 11pt;"
        "               selection-background-color: #F39C12; selection-color: white; }"
        "QHeaderView::section { background: #2E75B6; color: white; padding: 6px 10px;"
        "                        border: none; font-weight: bold; font-size: 11pt; }"
        "QStatusBar { background: #1F4E79; color: white; font-size: 10pt; }"
        "QLabel { font-size: 11pt; }"
        "QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox, QDateEdit { font-size: 11pt; }"
    );
#endif
}

void MainWindow::closeEvent(QCloseEvent *event)
{
#if defined(Q_OS_WIN)
    // Fenstergeometrie + Spaltenbreiten in Registry sichern
    QSettings s;
    s.setValue(QStringLiteral("mainwindow/geometry"), saveGeometry());
    s.setValue(QStringLiteral("mainwindow/state"),    saveState());
    if (m_fahrtenTab && m_fahrtenTab->tableWidget())
        saveHeaderState(QStringLiteral("fahrten/header"),
                        m_fahrtenTab->tableWidget()->horizontalHeader());
    if (m_adressTab && m_adressTab->tableWidget())
        saveHeaderState(QStringLiteral("adressen/header"),
                        m_adressTab->tableWidget()->horizontalHeader());
    if (m_fahrerTab && m_fahrerTab->tableWidget())
        saveHeaderState(QStringLiteral("fahrer/header"),
                        m_fahrerTab->tableWidget()->horizontalHeader());
#endif
    m_einstellTab->save();
    m_syncMgr->stopSync();
    m_syncMgr->closeDb();
    m_db->close();
    event->accept();
#if defined(Q_OS_ANDROID)
    // Task aus Back-Stack entfernen (verhindert Android-Neustart nach SIGKILL)
    QJniObject activity = QJniObject::callStaticObjectMethod(
        "org/qtproject/qt/android/QtNative",
        "activity",
        "()Landroid/app/Activity;");
    if (activity.isValid())
        activity.callMethod<void>("finishAndRemoveTask");
    // SIGKILL ist uncatchable – ART/atexit-Handler können exit(0) abfangen, SIGKILL nicht
    kill(getpid(), SIGKILL);
#endif
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
#if defined(Q_OS_ANDROID)
    if (event->key() == Qt::Key_Back || event->key() == Qt::Key_Escape) {
        if (m_drawer && m_drawer->isOpen()) {
            m_drawer->closeDrawer();
            event->accept();
            return;
        }
        if (m_tabs->currentIndex() != 0) {
            setAndroidPage(0);
            event->accept();
            return;
        }
        close();
        event->accept();
        return;
    }
#endif
    QMainWindow::keyPressEvent(event);
}

void MainWindow::onTabChanged(int /*index*/)
{
    QWidget *w = m_tabs->currentWidget();

#if defined(Q_OS_ANDROID)
#if defined(Q_OS_ANDROID)
    if (m_appBarBridge) {
        QString title;
        if      (w == ui->tabFahrten)       title = tr("Fahrten");
        else if (w == ui->tabAdressen)      title = tr("Adressen");
        else if (w == ui->tabFahrer)        title = tr("Fahrer");
        else if (w == ui->tabEinstellungen) title = tr("Einstellungen");
        else                                title = tr("Fahrtenbuch");
        m_appBarBridge->setPageTitle(title);
    }
#endif
    {
        int drawerIdx = 0;
        if      (w == ui->tabFahrten)       drawerIdx = 0;
        else if (w == ui->tabAdressen)      drawerIdx = 1;
        else if (w == ui->tabFahrer)        drawerIdx = 2;
        else if (w == ui->tabEinstellungen) drawerIdx = 3;
        updateDrawerSelection(drawerIdx);
    }
#endif

    if      (w == ui->tabFahrten)       m_fahrtenTab->refresh();
    else if (w == ui->tabAdressen)      m_adressTab->refresh();
    else if (w == ui->tabFahrer)        m_fahrerTab->refresh();
    else if (w == ui->tabEinstellungen) m_einstellTab->load();
    else if (w == ui->tabExport)        m_exportTab->refresh();
}

void MainWindow::onDataChanged()
{
    m_hasChanges = m_fahrtenTab->hasUnsavedChanges()
                || m_adressTab->hasUnsavedChanges()
                || m_fahrerTab->hasUnsavedChanges();
    m_statusLabel->setText(m_hasChanges
        ? tr("Aenderungen vorhanden")
        : tr("Alle Daten gespeichert."));
}

void MainWindow::onSettingsChanged()
{
    Einstellungen e = m_db->getEinstellungen();
    m_distSvc->setApiKey(e.apiKeyDistance);
    m_syncMgr->applySettings(e);

    bool mehrere = e.mehrerefahrer;
    int fahrerTabIdx = m_tabs->indexOf(ui->tabFahrer);
    if (mehrere && fahrerTabIdx == -1) {
        int adresseIdx = m_tabs->indexOf(ui->tabAdressen);
        m_tabs->insertTab(adresseIdx + 1, ui->tabFahrer, tr("Fahrer"));
    } else if (!mehrere && fahrerTabIdx != -1) {
        m_tabs->removeTab(fahrerTabIdx);
    }
    m_fahrtenTab->setFahrerVisible(mehrere);
    ui->actionFahrer->setVisible(mehrere);
    m_fahrtenTab->refresh();
    m_exportTab->refresh();

    // Sync-Menü bleibt immer sichtbar (DB-Übertragung sync-mode-unabhängig)

#if defined(Q_OS_ANDROID)
    if (m_drawer)
        m_drawer->setItemVisible(2, mehrere);
#endif

    const bool langChanged = (e.language != m_lastKnownLanguage);
    Q_UNUSED(langChanged)  // wird nur im Windows-Zweig (#else) verwendet
    m_lastKnownLanguage = e.language;

    // Sprache in QSettings schreiben – main.cpp liest von dort beim nächsten Start
    {
        QSettings s;
        s.setValue(QStringLiteral("app/language"), e.language);
    }

#if defined(Q_OS_ANDROID)
    // AppBar: kurz "✓ Gespeichert" zeigen, dann Sync-Status wiederherstellen
    // (Snackbar wird bereits von EinstellungenBridge::save() gezeigt — kein Duplikat)
    if (m_appBarBridge) {
        m_appBarBridge->setSyncStatus(tr("✓ Gespeichert"));
        QTimer::singleShot(2000, this, [this]() {
            if (m_appBarBridge)
                m_appBarBridge->setSyncStatus(
                    m_syncMgr && m_syncMgr->isRunning() ? tr("Sync: WLAN") : tr("Sync: aus"));
        });
    }
#else
    m_statusLabel->setText(tr("Einstellungen gespeichert."));
    if (langChanged)
        QMessageBox::information(this,
            tr("Sprache geändert"),
            tr("Die Sprachänderung wird nach dem nächsten App-Start wirksam."));
#endif
}

void MainWindow::onAutoSave()
{
    m_einstellTab->save();
}

void MainWindow::onSaveAll()
{
    onAutoSave();
}

void MainWindow::onSyncStart()
{
    if (m_syncMgr->isRunning()) {
        m_syncMgr->stopSync();
#if defined(Q_OS_ANDROID)
        if (m_appBarBridge) m_appBarBridge->setSyncStatus(tr("Sync: aus"));
#endif
    } else {
        m_syncMgr->startSync();
#if defined(Q_OS_ANDROID)
        if (m_appBarBridge) m_appBarBridge->setSyncStatus(
            tr("Sync: WLAN"));
#endif
    }
}

void MainWindow::onSyncStatus()
{
    QString status = m_syncMgr->isRunning()
        ? tr("WLAN-Sync aktiv (UDP:%1 / TCP:%2)")
              .arg(m_db->getEinstellungen().wifiUdpPort)
              .arg(m_db->getEinstellungen().wifiTcpPort)
        : tr("Kein Sync aktiv.");
#if defined(Q_OS_ANDROID)
    Snackbar::show(this, status, 3000);
#else
    QMessageBox::information(this, tr("Sync-Status"), status);
#endif
}

void MainWindow::onSyncFinished(bool ok, const QString &msg)
{
#if defined(Q_OS_ANDROID)
#if defined(Q_OS_ANDROID)
    if (m_appBarBridge) m_appBarBridge->setSyncStatus(ok ? tr("Sync OK") : tr("Sync Fehler"));
#endif
#else
    if (m_syncLabel) m_syncLabel->setText(ok ? tr("Sync OK") : tr("Sync Fehler"));
#endif
    if (ok) {
        if (!msg.isEmpty()) m_statusLabel->setText(msg);
    } else {
#if defined(Q_OS_ANDROID)
        Snackbar::show(this, tr("⚠ Synchronisation fehlgeschlagen"), 4000);
#else
        if (!msg.isEmpty())
            QMessageBox::warning(this, tr("Synchronisation"), msg);
#endif
    }
}

void MainWindow::onSyncProgress(const QString &msg)
{
    m_statusLabel->setText(msg);
}

void MainWindow::onNavFahrten()       { m_tabs->setCurrentWidget(ui->tabFahrten); }
void MainWindow::onNavAdressen()      { m_tabs->setCurrentWidget(ui->tabAdressen); }
void MainWindow::onNavFahrer()        { m_tabs->setCurrentWidget(ui->tabFahrer); }
void MainWindow::onNavEinstellungen() { m_tabs->setCurrentWidget(ui->tabEinstellungen); }
void MainWindow::onNavExport()        { m_tabs->setCurrentWidget(ui->tabExport); }

void MainWindow::showAbout()
{
#if defined(Q_OS_ANDROID)
    new AboutDialog(this);  // QQuickWidget öffnet sich im Konstruktor, QDialog bleibt hidden
#else
    AboutDialog dlg(this); dlg.exec();
#endif
}
void MainWindow::showQtAbout() { QMessageBox::aboutQt(this, tr("Ueber Qt")); }

// ── Datenbank exportieren ─────────────────────────────────────────────────
// Android: DB liegt unter /storage/emulated/0/Fahrtenbuch/ – direkt per USB erreichbar.
// Nur Checkpoint erzwingen (WAL in Hauptdatei schreiben) und Pfad anzeigen.
// Windows: Datei-Dialog zum Speichern.
// ── Datenbank importieren ─────────────────────────────────────────────────
// Android: DB liegt am oeffentlichen Pfad – einfach direkt per QFile::copy ersetzen.
// Windows: Datei-Dialog zum Oeffnen.
void MainWindow::showSyncLog()
{
    if (!m_syncMgr->dbSync())
        m_syncMgr->initDb();

    if (!m_syncMgr->dbSync()) {
        m_statusLabel->setText(tr("Sync-Protokoll: Datenbank nicht verfügbar"));
        return;
    }
    SyncLogView *dlg = new SyncLogView(m_syncMgr->dbSync(), this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
#if defined(Q_OS_WIN)
    dlg->show();
#endif
}


void MainWindow::showHelp()
{
    auto *help = new HelpWindow(this);
    help->setAttribute(Qt::WA_DeleteOnClose);
#if defined(Q_OS_WIN)
    help->show();
#endif
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
#if defined(Q_OS_ANDROID)
    if (event->type() == QEvent::Resize && m_drawer) {
        auto *w = static_cast<QWidget*>(obj);
        m_drawer->setGeometry(0, 56, w->width(), w->height() - 56);
    }
#else
    Q_UNUSED(obj) Q_UNUSED(event)
#endif
    return QMainWindow::eventFilter(obj, event);
}


#if defined(Q_OS_ANDROID)

// ─── setupAndroidNav ───────────────────────────────────────────────────────────
void MainWindow::setupAndroidNav()
{
    m_tabs->tabBar()->hide();

    auto *oldCentral = centralWidget();
    auto *newCentral = new QWidget();
    auto *rootLay    = new QVBoxLayout(newCentral);
    rootLay->setContentsMargins(0, 0, 0, 0);
    rootLay->setSpacing(0);

    // ── AppBarBridge + AppBar QML (56px, im Layout) ───────────────────────────
    m_appBarBridge = new AppBarBridge(this);

    m_appBarQml = new QQuickWidget(newCentral);
    m_appBarQml->setFixedHeight(56);
    m_appBarQml->setAttribute(Qt::WA_AcceptTouchEvents);
    m_appBarQml->setResizeMode(QQuickWidget::SizeRootObjectToView);
    m_appBarQml->rootContext()->setContextProperty(
        QStringLiteral("appBarBridge"), m_appBarBridge);
    m_appBarQml->setSource(QUrl(QStringLiteral("qrc:/AppBar.qml")));
    // Verbindung einmalig herstellen — UniqueConnection verhindert Duplikate
    auto connectAppBar = [this]() {
        if (auto *root = m_appBarQml->rootObject())
            connect(root, SIGNAL(menuToggled()), this, SLOT(onBurgerClicked()),
                    Qt::UniqueConnection);
    };
    connect(m_appBarQml, &QQuickWidget::statusChanged, this,
        [this, connectAppBar](QQuickWidget::Status s) {
            if (s == QQuickWidget::Error)
                for (const auto &e : m_appBarQml->errors())
                    qWarning() << "AppBar QML:" << e.toString();
            if (s == QQuickWidget::Ready) connectAppBar();
        });
    if (m_appBarQml->status() == QQuickWidget::Ready) connectAppBar();
    // Sync-Log freischalten wenn Bridge das Signal sendet (7x Titel-Tap)
    connect(m_appBarBridge, &AppBarBridge::syncLogActivated, this, [this]() {
        if (m_drawer) {
            m_drawer->setItemVisible(7, true);   // 7 = Sync-Log
            // Kurze visuelle Bestätigung im Sync-Status
            m_appBarBridge->setSyncStatus(tr("🔓 Sync-Log aktiv"));
            QTimer::singleShot(2000, this, [this]() {
                m_appBarBridge->setSyncStatus(
                    m_syncMgr && m_syncMgr->isRunning() ? tr("Sync: WLAN") : tr("Sync: aus"));
            });
        }
#if defined(Q_OS_ANDROID)
        m_einstellTab->setOrsUnlocked();
#endif
    });
    rootLay->addWidget(m_appBarQml);

    // ── Tabs ──────────────────────────────────────────────────────────────────
    m_tabs->setParent(newCentral);
    rootLay->addWidget(m_tabs, 1);

    setCentralWidget(newCentral);
    oldCentral->deleteLater();
    statusBar()->hide();

    // Sync-Label-Dummy
    m_syncLabel = new QLabel(QString(), this);

    // ── DrawerWidget (Qt Widgets, zuverlässig als Overlay) ────────────────────
    // DrawerWidget als Kind von newCentral, positioniert ab y=56 (unter AppBar).
    // Nutzt absolute Positionierung — kein Conflict mit QTabWidget-Innenabstand.
    m_drawer = new DrawerWidget(newCentral);

    Einstellungen e = m_db->getEinstellungen();
    m_drawer->addPage(QString::fromUtf8("\xF0\x9F\x9A\x97"), tr("Fahrten"));       // 🚗 0
    m_drawer->addPage(QString::fromUtf8("\xF0\x9F\x93\x8D"), tr("Adressen"));      // 📍 1
    m_drawer->addPage(QString::fromUtf8("\xF0\x9F\x91\xA4"), tr("Fahrer"));        // 👤 2
    m_drawer->addPage(QString::fromUtf8("\xE2\x9A\x99"),      tr("Einstellungen")); // ⚙  3
    m_drawer->addSeparator();                                                        //    4
    m_drawer->addPage(QString::fromUtf8("\xF0\x9F\x93\xA4"), tr("Export"));        // 📤 5
    m_drawer->addPage(QString::fromUtf8("\xE2\x9D\x93"),      tr("Hilfe"));         // ❓ 6
    m_drawer->addPage(QString::fromUtf8("\xF0\x9F\x94\x84"), tr("Sync-Log"));      // 🔄 7
    m_drawer->addPage(QString::fromUtf8("\xE2\x84\xB9"),      tr("Info"));          // ℹ  8
    m_drawer->addSeparator();                                                        //    9
    m_drawer->addPage(QString::fromUtf8("\xE2\x9C\x96"),      tr("Beenden"));       // ✖  10

    if (!e.mehrerefahrer)
        m_drawer->setItemVisible(2, false);
    // Sync-Log (Index 7) im Release-Build verstecken – per 7x Titel-Tap freischaltbar
#ifndef QT_DEBUG
    m_drawer->setItemVisible(7, false);
#endif
    m_drawer->setCurrentIndex(0);

    connect(m_drawer, &DrawerWidget::pageSelected, this, [this](int idx) {
        switch (idx) {
            case 0: case 1: case 2: case 3: setAndroidPage(idx); break;
            case 5: showExportView(); break;
            case 6: { HelpWindow *h = new HelpWindow(this);
                      h->setAttribute(Qt::WA_DeleteOnClose);
#if defined(Q_OS_WIN)
                      h->show();
#endif
                    } break;
            case 7: showSyncLog();  break;
            case 8: showAbout();    break;
            case 10: close();        break;
            default: break;
        }
    });

    // ── Swipe ─────────────────────────────────────────────────────────────────
    newCentral->setAttribute(Qt::WA_AcceptTouchEvents);
    if (m_swipeFilter) {
        m_swipeFilter->installOnParent(newCentral);
        connect(m_swipeFilter, &SwipeFilter::swipedLeft, this, [this]() {
            if (m_drawer && m_drawer->isOpen()) return;
            int cur = m_tabs->currentIndex();
            if (cur < m_tabs->count() - 1) m_tabs->setCurrentIndex(cur + 1);
        });
        connect(m_swipeFilter, &SwipeFilter::swipedRight, this, [this]() {
            if (m_drawer && m_drawer->isOpen()) { m_drawer->closeDrawer(); return; }
            int cur = m_tabs->currentIndex();
            if (cur > 0) m_tabs->setCurrentIndex(cur - 1);
        });
    }
    newCentral->installEventFilter(this);
}


#if defined(Q_OS_ANDROID)
void MainWindow::onBurgerClicked()
{
    if (m_drawer) m_drawer->toggle();
}

void MainWindow::onDrawerClosed()
{
    // handled by DrawerWidget internally
}

void MainWindow::showExportView()
{
    // ExportBridge jedes Mal neu – vermeidet angehäufte Signal-Verbindungen
    delete m_exportBridge;
    m_exportBridge = new ExportBridge(m_db, this);

    // Neues QQuickWidget jedes Mal – exakt wie SyncLogView.
    // QQuickWidget teilt den EGL-Context aller anderen QQuickWidgets → kein Konflikt.
    auto *pw = qobject_cast<QWidget*>(this);
    auto *qw = new QQuickWidget(pw);
    qw->setAttribute(Qt::WA_DeleteOnClose);
    qw->setResizeMode(QQuickWidget::SizeRootObjectToView);
    qw->rootContext()->setContextProperty(QStringLiteral("exportBridge"), m_exportBridge);
    m_exportBridge->reset();  // Status zurücksetzen beim Öffnen

    connect(qw, &QQuickWidget::statusChanged, qw,
        [qw](QQuickWidget::Status s) {
            if (s == QQuickWidget::Error)
                for (const auto &e : qw->errors())
                    qWarning() << "ExportView QML:" << e.toString();
        });

    // Alte Verbindungen trennen bevor neue gesetzt werden (Bridge ist persistent)
    disconnect(m_exportBridge, &ExportBridge::snackbarRequested, nullptr, nullptr);
    disconnect(m_exportBridge, &ExportBridge::closeRequested,    nullptr, nullptr);

    connect(m_exportBridge, &ExportBridge::snackbarRequested,
        this, [qw](const QString &text, int ms) {
            if (qw) Snackbar::show(qw, text, ms);
        });

    connect(m_exportBridge, &ExportBridge::closeRequested,
        qw, &QWidget::deleteLater);

    qw->setSource(QUrl(QStringLiteral("qrc:/ExportView.qml")));
    if (pw) qw->setGeometry(0, 56, pw->width(), pw->height() - 56);
    qw->show();
    qw->raise();
}
#endif

void MainWindow::updateDrawerSelection(int pageIdx)
{
    if (m_drawer) m_drawer->setCurrentIndex(pageIdx);
}


void MainWindow::onDrawerPageSelected(int idx)
{
    switch (idx) {
        case 0: case 1: case 2: case 3:
            setAndroidPage(idx);
            break;
        case 5:  { HelpWindow *h = new HelpWindow(this);
                   h->setAttribute(Qt::WA_DeleteOnClose);
#if defined(Q_OS_WIN)
                   h->show();
#endif
                 } break;
        case 6:  showSyncLog();  break;
        case 7:  showAbout();    break;
        case 9:  close();        break;
        default: break;
    }
}
void MainWindow::setAndroidPage(int index)
{
    Einstellungen e = m_db->getEinstellungen();
    QWidget *target = nullptr;
    switch (index) {
        case 0: target = ui->tabFahrten;       break;
        case 1: target = ui->tabAdressen;      break;
        case 2: target = e.mehrerefahrer ? ui->tabFahrer : nullptr; break;
        case 3: target = ui->tabEinstellungen; break;
    }
    if (target) m_tabs->setCurrentWidget(target);
}
#endif // Q_OS_ANDROID

#if defined(Q_OS_ANDROID)
void MainWindow::checkAndRequestNearbyWifiPermission()
{
    // NEARBY_WIFI_DEVICES ist ab Android 13 (API 33) für UDP-Discovery Pflicht.
    // Unter API 33 ist die Permission nicht vorhanden – kein Aufruf nötig.
    QJniEnvironment env;
    jclass vClass = env->FindClass("android/os/Build$VERSION");
    int sdk = 0;
    if (vClass) {
        jfieldID f = env->GetStaticFieldID(vClass, "SDK_INT", "I");
        if (f) sdk = env->GetStaticIntField(vClass, f);
    }
    qDebug() << "[NearbyWifi] SDK_INT:" << sdk;
    if (sdk < 33) {
        qDebug() << "[NearbyWifi] API < 33 – NEARBY_WIFI_DEVICES nicht noetig";
        return;
    }

    // Prüfen ob Permission bereits erteilt
    const QString perm = QStringLiteral("android.permission.NEARBY_WIFI_DEVICES");
    QJniObject activity = QJniObject::callStaticObjectMethod(
        "org/qtproject/qt/android/QtNative", "activity", "()Landroid/app/Activity;");
    if (!activity.isValid()) return;

    const int GRANTED = 0; // PackageManager.PERMISSION_GRANTED
    int result = activity.callMethod<jint>(
        "checkSelfPermission",
        "(Ljava/lang/String;)I",
        QJniObject::fromString(perm).object<jstring>());

    if (result == GRANTED) {
        qDebug() << "[NearbyWifi] NEARBY_WIFI_DEVICES bereits gewaehrt";
        return;
    }

    qDebug() << "[NearbyWifi] Fordere NEARBY_WIFI_DEVICES an...";

    // requestPermissions() – System-Dialog erscheint
    QJniObject permsArray = QJniObject::callStaticObjectMethod(
        "java/lang/reflect/Array", "newInstance",
        "(Ljava/lang/Class;I)Ljava/lang/Object;",
        env->FindClass("java/lang/String"),
        jint(1));

    // Einfacher Weg: direkt als String-Array
    jobjectArray arr = env->NewObjectArray(
        1, env->FindClass("java/lang/String"), nullptr);
    env->SetObjectArrayElement(
        arr, 0, QJniObject::fromString(perm).object<jstring>());

    activity.callMethod<void>(
        "requestPermissions",
        "([Ljava/lang/String;I)V",
        arr,
        jint(42));  // requestCode 42 = NEARBY_WIFI

    env->DeleteLocalRef(arr);
    qDebug() << "[NearbyWifi] requestPermissions() aufgerufen";
}
#endif

// MOC fuer Q_OBJECT-Klassen (AppBarBridge) in diesem .cpp
#if defined(Q_OS_ANDROID)

void MainWindow::hideAllQuickWidgets()
{
    // Qt 6.8 / Android 16: Nur ein aktiver EGL-Context erlaubt.
    // Alle sichtbaren QQuickWidgets der App verstecken bevor ein QQuickView geöffnet wird.
    const auto allWidgets = findChildren<QQuickWidget*>();
    for (auto *w : allWidgets) {
        if (w->isVisible()) w->hide();
    }
}

void MainWindow::onApplicationStateChanged(Qt::ApplicationState state)
{
    if (!m_syncMgr || !m_syncMgr->isRunning()) return;

    if (state == Qt::ApplicationActive) {
        qDebug() << "[AppState] Vordergrund → resumeSync";
        m_syncMgr->resumeSync();
    } else if (state == Qt::ApplicationHidden
            || state == Qt::ApplicationSuspended) {
        qDebug() << "[AppState] Hintergrund (" << state << ") → pauseSync";
        m_syncMgr->pauseSync();
    }
    // ApplicationInactive ignorieren (Multitasking-Vorschau, Benachrichtigungen)
}

#include "mainwindow.moc"
#endif
