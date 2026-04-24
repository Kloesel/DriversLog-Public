#include "einstellungen.h"
#include "touchscroll.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QScrollArea>
#include <QFileDialog>
#include <QDate>
#include <QDebug>
#include <QWheelEvent>
#include <QMessageBox>

// Blockiert Mausrad-Events wenn das Widget keinen Fokus hat.
// Verhindert ungewollte Wertänderungen beim Scrollen der Einstellungsseite.
class WheelBlocker : public QObject {
public:
    explicit WheelBlocker(QObject *parent = nullptr) : QObject(parent) {}
protected:
    bool eventFilter(QObject *obj, QEvent *event) override {
        if (event->type() == QEvent::Wheel) {
            auto *w = qobject_cast<QWidget*>(obj);
            if (w && !w->hasFocus()) {
                event->ignore();
                return true;   // Event geschluckt
            }
        }
        return QObject::eventFilter(obj, event);
    }
};
#if defined(Q_OS_ANDROID)
#  include <QQuickView>
#  include <QQmlContext>
#  include "snackbar.h"
#endif

// ─── Android: EinstellungenBridge ────────────────────────────────────────────
// Exponiert alle Einstellungen als Q_PROPERTY an EinstellungenView.qml.
#if defined(Q_OS_ANDROID)
class EinstellungenBridge : public QObject
{
    Q_OBJECT

    // Combo-Modelle (listen)
    Q_PROPERTY(QVariantList monatModel    READ monatModel    CONSTANT)
    Q_PROPERTY(QVariantList syncModeModel READ syncModeModel CONSTANT)
    Q_PROPERTY(QVariantList languageModel READ languageModel CONSTANT)
    Q_PROPERTY(QVariantList fahrerModel  READ fahrerModel  NOTIFY fahrerModelChanged)
    Q_PROPERTY(QVariantList adressenModel READ adressenModel NOTIFY adressenModelChanged)

    // Initialdaten
    Q_PROPERTY(int     initMonat         MEMBER m_initMonat         CONSTANT)
    Q_PROPERTY(int     initJahr          MEMBER m_initJahr          CONSTANT)
    Q_PROPERTY(int     initFahrerId      MEMBER m_initFahrerId      CONSTANT)
    Q_PROPERTY(int     initAdresseId     MEMBER m_initAdresseId     CONSTANT)
    Q_PROPERTY(bool    initHinZurueck    MEMBER m_initHinZurueck    CONSTANT)
    Q_PROPERTY(bool    initMehrereFahrer MEMBER m_initMehrereFahrer CONSTANT)
    Q_PROPERTY(QString initApiKey        MEMBER m_initApiKey        CONSTANT)
    Q_PROPERTY(int     initSyncMode      MEMBER m_initSyncMode      CONSTANT)
    Q_PROPERTY(int     initUdpPort       MEMBER m_initUdpPort       CONSTANT)
    Q_PROPERTY(int     initTcpPort       MEMBER m_initTcpPort       CONSTANT)
    Q_PROPERTY(int     initLanguage      MEMBER m_initLanguage      CONSTANT)

public:
    explicit EinstellungenBridge(Database *db, SyncManager *syncMgr,
                                 QObject *parent = nullptr)
        : QObject(parent), m_db(db), m_syncMgr(syncMgr)
    {
        // Monat-Modell
        QVariantMap alle; alle[QStringLiteral("id")] = 0;
        alle[QStringLiteral("text")] = tr("Alle Monate");
        m_monatModel.append(alle);
        const QStringList monate = {
            tr("Januar"), tr("Februar"), tr("März"),     tr("April"),
            tr("Mai"),    tr("Juni"),    tr("Juli"),      tr("August"),
            tr("September"), tr("Oktober"), tr("November"), tr("Dezember")
        };
        for (int i = 0; i < 12; ++i) {
            QVariantMap m;
            m[QStringLiteral("id")]   = i + 1;
            m[QStringLiteral("text")] = monate[i];
            m_monatModel.append(m);
        }

        // Sync-Modell
        QVariantMap off; off[QStringLiteral("id")] = 0;
        off[QStringLiteral("text")] = tr("Deaktiviert");
        QVariantMap wifi; wifi[QStringLiteral("id")] = 1;
        wifi[QStringLiteral("text")] = tr("WLAN (automatisch)");
        m_syncModeModel.append(off);
        m_syncModeModel.append(wifi);

        // Sprach-Modell: 0=System, 1=Deutsch, 2=English, 3=Français, 4=Nederlands, 5=Español
        auto mkLang = [](int id, const QString &t) {
            QVariantMap m; m[QStringLiteral("id")] = id; m[QStringLiteral("text")] = t; return m;
        };
        m_languageModel.append(mkLang(0, tr("Systemsprache")));
        m_languageModel.append(mkLang(1, tr("Deutsch")));
        m_languageModel.append(mkLang(2, tr("English")));
        m_languageModel.append(mkLang(3, tr("Français")));
        m_languageModel.append(mkLang(4, tr("Nederlands")));
        m_languageModel.append(mkLang(5, tr("Español")));

        reloadFromDb();
    }

    QVariantList monatModel()    const { return m_monatModel;    }
    QVariantList syncModeModel() const { return m_syncModeModel; }
    QVariantList languageModel() const { return m_languageModel; }
    QVariantList fahrerModel()   const { return m_fahrerModel;   }
    QVariantList adressenModel() const { return m_adressenModel; }

    // Lädt aktuelle DB-Werte — wird von EinstellungenWidget::load() aufgerufen
    void reloadFromDb()
    {
        Einstellungen e = m_db->getEinstellungen();
        m_initMonat         = e.filterMonat;
        m_initJahr          = e.filterJahr > 0 ? e.filterJahr : QDate::currentDate().year();
        m_initFahrerId      = e.standardFahrerId;
        m_initAdresseId     = e.standardAdresseId;
        m_initHinZurueck    = e.standardHinZurueck;
        m_initMehrereFahrer = e.mehrerefahrer;
        m_initApiKey        = e.apiKeyDistance;
        m_initSyncMode      = (e.syncMode == QLatin1String("wifi")) ? 1 : 0;
        m_initUdpPort       = e.wifiUdpPort > 0 ? e.wifiUdpPort : 45455;
        m_initTcpPort       = e.wifiTcpPort > 0 ? e.wifiTcpPort : 45454;
        if      (e.language == QLatin1String("de")) m_initLanguage = 1;
        else if (e.language == QLatin1String("en")) m_initLanguage = 2;
        else if (e.language == QLatin1String("fr")) m_initLanguage = 3;
        else if (e.language == QLatin1String("nl")) m_initLanguage = 4;
        else if (e.language == QLatin1String("es")) m_initLanguage = 5;
        else                                        m_initLanguage = 0;

        // Fahrer-Modell
        m_fahrerModel.clear();
        QVariantMap ph; ph[QStringLiteral("id")] = 0;
        ph[QStringLiteral("text")] = tr("(kein)");
        m_fahrerModel.append(ph);
        for (const Fahrer &f : m_db->getAllFahrer()) {
            QVariantMap m;
            m[QStringLiteral("id")]   = f.id;
            m[QStringLiteral("text")] = f.isDefault ? tr("unbekannt") : f.name;
            m_fahrerModel.append(m);
        }
        emit fahrerModelChanged();

        // Adressen-Modell
        m_adressenModel.clear();
        QVariantMap aph; aph[QStringLiteral("id")] = 0;
        aph[QStringLiteral("text")] = tr("(keine)");
        m_adressenModel.append(aph);
        for (const Adresse &a : m_db->getAllAdressen()) {
            QVariantMap m;
            m[QStringLiteral("id")]   = a.id;
            m[QStringLiteral("text")] = a.anzeige();
            m_adressenModel.append(m);
        }
        emit adressenModelChanged();
    }

signals:
    void fahrerModelChanged();
    void adressenModelChanged();
    void settingsSaved();
    void cancelRequested();
    void snackbarRequested(QString text, int ms);

public slots:
    Q_INVOKABLE void cancel() { emit cancelRequested(); }

    Q_INVOKABLE void save(int monat, int jahr, int fahrerId, int adresseId,
                          bool hinZurueck, bool mehrereFahrer,
                          const QString &apiKey, int syncMode,
                          int udpPort, int tcpPort, int language)
    {
        const QString oldLanguage = m_db->getEinstellungen().language;
        Einstellungen e = m_db->getEinstellungen();
        e.filterMonat        = monat;
        e.filterJahr         = jahr;
        e.standardFahrerId   = fahrerId;
        e.standardAdresseId  = adresseId;
        e.standardHinZurueck = hinZurueck;
        e.mehrerefahrer      = mehrereFahrer;
        e.apiKeyDistance     = apiKey.trimmed();
        e.syncMode           = (syncMode == 1) ? QStringLiteral("wifi")
                                               : QStringLiteral("off");
        e.wifiUdpPort        = udpPort;
        e.wifiTcpPort        = tcpPort;
        if      (language == 1) e.language = QStringLiteral("de");
        else if (language == 2) e.language = QStringLiteral("en");
        else if (language == 3) e.language = QStringLiteral("fr");
        else if (language == 4) e.language = QStringLiteral("nl");
        else if (language == 5) e.language = QStringLiteral("es");
        else                    e.language.clear(); // Systemsprache

        const bool langChanged = (e.language != oldLanguage);
        m_db->saveEinstellungen(e);
        if (m_syncMgr) m_syncMgr->applySettings(e);

        if (langChanged)
            emit snackbarRequested(
                tr("Einstellungen gespeichert – App neu starten um Sprache zu wechseln"), 4000);
        else
            emit snackbarRequested(tr("Einstellungen gespeichert"), 2500);
        emit settingsSaved();
    }

private:
    Database    *m_db;
    SyncManager *m_syncMgr;

    QVariantList m_monatModel;
    QVariantList m_syncModeModel;
    QVariantList m_fahrerModel;
    QVariantList m_adressenModel;

    int     m_initMonat         = 0;
    int     m_initJahr          = 2024;
    int     m_initFahrerId      = 0;
    int     m_initAdresseId     = 0;
    bool    m_initHinZurueck    = false;
    bool    m_initMehrereFahrer = true;
    QString m_initApiKey;
    int     m_initSyncMode      = 0;
    int     m_initUdpPort       = 45455;
    int     m_initTcpPort       = 45454;
    int     m_initLanguage      = 0;
    QVariantList m_languageModel;
};
#endif // Q_OS_ANDROID

// ─── Konstruktor ─────────────────────────────────────────────────────────────

EinstellungenWidget::EinstellungenWidget(Database *db, SyncManager *syncMgr,
                                         QWidget *parent)
    : QWidget(parent), m_db(db), m_syncMgr(syncMgr)
{
#if defined(Q_OS_ANDROID)
    // ── Android: QML-View ─────────────────────────────────────────────────────
    m_bridge = new EinstellungenBridge(db, syncMgr, this);

    connect(m_bridge, &EinstellungenBridge::settingsSaved,
            this, &EinstellungenWidget::onSave);
    connect(m_bridge, &EinstellungenBridge::cancelRequested,
            this, &EinstellungenWidget::onCancel);
    connect(m_bridge, &EinstellungenBridge::snackbarRequested,
            this, [this](const QString &text, int ms) {
                Snackbar::show(this, text, ms);
            });

    m_qmlView = new QQuickWidget(this);
    m_qmlView->setAttribute(Qt::WA_AcceptTouchEvents);
    m_qmlView->setResizeMode(QQuickWidget::SizeRootObjectToView);
    m_qmlView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_qmlView->rootContext()->setContextProperty(
        QStringLiteral("settingsBridge"), m_bridge);
    m_qmlView->setSource(QUrl(QStringLiteral("qrc:/EinstellungenView.qml")));

    connect(m_qmlView, &QQuickWidget::statusChanged, this,
            [this](QQuickWidget::Status s) {
                if (s == QQuickWidget::Error)
                    for (const auto &e : m_qmlView->errors())
                        qWarning() << "EinstellungenView QML:" << e.toString();
            });

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_qmlView);

#else
    // ── Desktop: Widget-basiert (unverändert) ─────────────────────────────────

    // Datenbank-Pfad
    auto *dbGroup  = new QGroupBox(tr("Datenbank"), this);
    auto *dbLayout = new QHBoxLayout(dbGroup);
    m_dbPathEdit   = new QLineEdit(this);
    m_dbPathEdit->setPlaceholderText(tr("Standard (App-Verzeichnis)"));
    auto *chooseBtn = new QPushButton(tr("Durchsuchen..."), this);
    chooseBtn->setMinimumWidth(110);
    dbLayout->addWidget(m_dbPathEdit);
    dbLayout->addWidget(chooseBtn);
    connect(chooseBtn, &QPushButton::clicked,
            this, &EinstellungenWidget::onChooseDbPath);

    // Filter
    auto *filterGroup  = new QGroupBox(tr("Standardfilter"), this);
    auto *filterLayout = new QFormLayout(filterGroup);
    m_monatCb = new QComboBox(this);
    m_monatCb->addItem(tr("Alle Monate"), 0);
    const QStringList monate = {
        tr("Januar"), tr("Februar"), tr("März"),     tr("April"),
        tr("Mai"),    tr("Juni"),    tr("Juli"),      tr("August"),
        tr("September"), tr("Oktober"), tr("November"), tr("Dezember")
    };
    for (int i = 0; i < 12; ++i)
        m_monatCb->addItem(monate[i], i + 1);
    m_jahrSpin = new QSpinBox(this);
    m_jahrSpin->setRange(2000, 2099);
    m_jahrSpin->setValue(QDate::currentDate().year());
    filterLayout->addRow(tr("Monat:"), m_monatCb);
    filterLayout->addRow(tr("Jahr:"),  m_jahrSpin);

    // Standard-Werte
    auto *defaultGroup  = new QGroupBox(tr("Standard-Werte"), this);
    auto *defaultLayout = new QFormLayout(defaultGroup);
    m_standardFahrerCb  = new QComboBox(this);
    m_standardAdresseCb = new QComboBox(this);
    m_hinZurueckCb      = new QCheckBox(tr("Hin und Zurück"), this);
    m_mehrereFahrerCb   = new QCheckBox(tr("Mehrere Fahrer"), this);
    m_stdFahrerLabel    = new QLabel(tr("Standard-Fahrer:"), this);
    defaultLayout->addRow(m_stdFahrerLabel,       m_standardFahrerCb);
    defaultLayout->addRow(tr("Standard-Adresse:"), m_standardAdresseCb);
    defaultLayout->addRow(QString(),               m_hinZurueckCb);
    defaultLayout->addRow(QString(),               m_mehrereFahrerCb);
    connect(m_mehrereFahrerCb, &QCheckBox::checkStateChanged,
            this, &EinstellungenWidget::onMehrereFahrerChanged);

    // Distanzservice
    auto *apiGroup  = new QGroupBox(tr("Distanzberechnung"), this);
    auto *apiLayout = new QFormLayout(apiGroup);
    auto *apiHint   = new QLabel(tr("Ohne Key: OSRM (kostenlos, kein Account).\n"
                                    "Mit ORS API-Key: 500 Abfragen/Tag.\n"
                                    "Key unter openrouteservice.org registrieren."), this);
    apiHint->setStyleSheet("color:#666;font-size:9pt;");
    apiHint->setWordWrap(true);
    apiLayout->addRow(apiHint);
    m_apiKeyEdit    = new QLineEdit(this);
    m_apiKeyEdit->setPlaceholderText(tr("ORS API-Key (optional)"));
    m_apiKeyEdit->setEchoMode(QLineEdit::Password);
    apiLayout->addRow(tr("API-Key:"), m_apiKeyEdit);

    // Sprache
    auto *langGroup  = new QGroupBox(tr("Sprache"), this);
    auto *langLayout = new QFormLayout(langGroup);
    m_languageCb     = new QComboBox(this);
    m_languageCb->addItem(tr("Systemsprache"), QString());
    m_languageCb->addItem(tr("Deutsch"),       QStringLiteral("de"));
    m_languageCb->addItem(tr("English"),       QStringLiteral("en"));
    m_languageCb->addItem(tr("Français"),      QStringLiteral("fr"));
    m_languageCb->addItem(tr("Nederlands"),    QStringLiteral("nl"));
    m_languageCb->addItem(tr("Español"),       QStringLiteral("es"));
    langLayout->addRow(tr("App-Sprache:"), m_languageCb);
    m_langRestartLabel = new QLabel(
        tr("Sprachänderung wird nach App-Neustart wirksam"), this);
    m_langRestartLabel->setWordWrap(true);
    m_langRestartLabel->setStyleSheet("color: #888; font-size: 10px;");
    m_langRestartLabel->hide();
    langLayout->addRow(m_langRestartLabel);
    connect(m_languageCb, &QComboBox::currentIndexChanged,
            this, [this](int) {
                m_langRestartLabel->setVisible(true);
            });

    // Synchronisation
    auto *syncGroup  = new QGroupBox(tr("Synchronisation"), this);
    auto *syncLayout = new QVBoxLayout(syncGroup);
    auto *modeRow    = new QHBoxLayout();
    auto *modeLabel  = new QLabel(tr("Modus:"), this);
    m_syncModeCb     = new QComboBox(this);
    m_syncModeCb->addItem(tr("Deaktiviert"),          "off");
    m_syncModeCb->addItem(tr("WLAN (automatisch)"),   "wifi");
    modeRow->addWidget(modeLabel);
    modeRow->addWidget(m_syncModeCb);
    modeRow->addStretch();
    syncLayout->addLayout(modeRow);

    m_wifiGroup      = new QGroupBox(tr("WLAN-Ports (Standard empfohlen)"), this);
    auto *wifiLayout = new QFormLayout(m_wifiGroup);
    m_udpPortSpin    = new QSpinBox(this);
    m_udpPortSpin->setRange(1024, 65535);
    m_udpPortSpin->setValue(45455);
    m_tcpPortSpin    = new QSpinBox(this);
    m_tcpPortSpin->setRange(1024, 65535);
    m_tcpPortSpin->setValue(45454);
    wifiLayout->addRow(tr("UDP-Broadcast:"), m_udpPortSpin);
    wifiLayout->addRow(tr("TCP-Transfer:"),  m_tcpPortSpin);
    m_wifiGroup->setVisible(false);
    syncLayout->addWidget(m_wifiGroup);

    // Mausrad-Schutz: SpinBoxen und ComboBoxen nur mit Fokus steuerbar.
    // Verhindert ungewollte Wertänderungen beim Scrollen der Einstellungsseite.
    auto *wheelBlocker = new WheelBlocker(this);
    const auto spinBoxes = { m_jahrSpin, m_udpPortSpin, m_tcpPortSpin };
    for (auto *w : spinBoxes) {
        w->setFocusPolicy(Qt::StrongFocus);
        w->installEventFilter(wheelBlocker);
    }
    const auto comboBoxes = { m_monatCb, m_standardFahrerCb,
                               m_standardAdresseCb, m_languageCb, m_syncModeCb };
    for (auto *w : comboBoxes) {
        w->setFocusPolicy(Qt::StrongFocus);
        w->installEventFilter(wheelBlocker);
    }

    connect(m_syncModeCb, &QComboBox::currentTextChanged,
            this, &EinstellungenWidget::onSyncModeChanged);

    // Speichern-Button
    auto *saveBtn = new QPushButton(tr("Einstellungen speichern"), this);
    connect(saveBtn, &QPushButton::clicked,
            this, &EinstellungenWidget::onSave);

    // Haupt-Layout
    auto *content = new QWidget(this);
    auto *vbox    = new QVBoxLayout(content);
    vbox->addWidget(dbGroup);
    vbox->addWidget(filterGroup);
    vbox->addWidget(defaultGroup);
    vbox->addWidget(apiGroup);
    vbox->addWidget(langGroup);
    vbox->addWidget(syncGroup);
    vbox->addStretch();
    vbox->addWidget(saveBtn);

    auto *scroll = new QScrollArea(this);
    scroll->setWidget(content);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    enableTouchScroll(scroll);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(scroll);
#endif
}

// ── Laden / Speichern ─────────────────────────────────────────────────────────

void EinstellungenWidget::load()
{
#if defined(Q_OS_ANDROID)
    if (m_bridge) m_bridge->reloadFromDb();
#else
    Einstellungen e = m_db->getEinstellungen();

    m_dbPathEdit->setText(e.databasePath);

    int monatIdx = m_monatCb->findData(e.filterMonat);
    m_monatCb->setCurrentIndex(monatIdx >= 0 ? monatIdx : 0);
    m_jahrSpin->setValue(e.filterJahr > 0 ? e.filterJahr : QDate::currentDate().year());

    m_standardFahrerCb->clear();
    m_standardFahrerCb->addItem(tr("(kein)"), 0);
    for (const Fahrer &f : m_db->getAllFahrer())
        m_standardFahrerCb->addItem(f.isDefault ? tr("unbekannt") : f.name, f.id);

    m_standardAdresseCb->clear();
    m_standardAdresseCb->addItem(tr("(keine)"), 0);
    for (const Adresse &a : m_db->getAllAdressen())
        m_standardAdresseCb->addItem(a.anzeige(), a.id);

    int fahrerIdx = m_standardFahrerCb->findData(e.standardFahrerId);
    m_standardFahrerCb->setCurrentIndex(fahrerIdx >= 0 ? fahrerIdx : 0);
    int adresseIdx = m_standardAdresseCb->findData(e.standardAdresseId);
    m_standardAdresseCb->setCurrentIndex(adresseIdx >= 0 ? adresseIdx : 0);

    m_hinZurueckCb->setChecked(e.standardHinZurueck);
    m_mehrereFahrerCb->setChecked(e.mehrerefahrer);
    m_stdFahrerLabel->setVisible(e.mehrerefahrer);
    m_standardFahrerCb->setVisible(e.mehrerefahrer);

    m_apiKeyEdit->setText(e.apiKeyDistance);

    int modeIdx = m_syncModeCb->findData(e.syncMode);
    m_syncModeCb->setCurrentIndex(modeIdx >= 0 ? modeIdx : 0);
    m_udpPortSpin->setValue(e.wifiUdpPort > 0 ? e.wifiUdpPort : 45455);
    m_tcpPortSpin->setValue(e.wifiTcpPort > 0 ? e.wifiTcpPort : 45454);
    updateWifiWidgetsVisibility(e.syncMode == "wifi");

    int langIdx = m_languageCb->findData(e.language);
    m_languageCb->setCurrentIndex(langIdx >= 0 ? langIdx : 0);
    m_langRestartLabel->hide();
#endif
}

void EinstellungenWidget::save()
{
#if defined(Q_OS_ANDROID)
    // Android: Speichern wird direkt vom QML-Formular via bridge.save() ausgelöst.
    // Diese Methode wird vom MainWindow aufgerufen (z.B. beim Tab-Wechsel) —
    // auf Android ist der Zustand bereits persistent sobald der Nutzer "Speichern" tippt.
    Q_UNUSED(this)
#else
    Einstellungen e = m_db->getEinstellungen();
    e.databasePath       = m_dbPathEdit->text().trimmed();
    e.filterMonat        = m_monatCb->currentData().toInt();
    e.filterJahr         = m_jahrSpin->value();
    e.standardFahrerId   = m_standardFahrerCb->currentData().toInt();
    e.standardAdresseId  = m_standardAdresseCb->currentData().toInt();
    e.standardHinZurueck = m_hinZurueckCb->isChecked();
    e.mehrerefahrer      = m_mehrereFahrerCb->isChecked();
    e.apiKeyDistance     = m_apiKeyEdit->text().trimmed();
    e.syncMode           = m_syncModeCb->currentData().toString();
    e.wifiUdpPort        = m_udpPortSpin->value();
    e.wifiTcpPort        = m_tcpPortSpin->value();
    e.language           = m_languageCb->currentData().toString();
    m_db->saveEinstellungen(e);
    if (m_syncMgr) m_syncMgr->applySettings(e);
#endif
}

// ── Private Slots ─────────────────────────────────────────────────────────────

void EinstellungenWidget::onSave()
{
#if !defined(Q_OS_ANDROID)
    save();
#endif
    emit settingsChanged();
}

void EinstellungenWidget::onCancel()
{
    emit cancelRequested();
}

#if !defined(Q_OS_ANDROID)
void EinstellungenWidget::onSyncModeChanged(const QString &)
{
    updateWifiWidgetsVisibility(
        m_syncModeCb->currentData().toString() == QLatin1String("wifi"));
}

void EinstellungenWidget::onChooseDbPath()
{
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Datenbankpfad wählen"), m_dbPathEdit->text(),
        tr("SQLite-Datenbanken (*.db);;Alle Dateien (*)"));
    if (!path.isEmpty()) m_dbPathEdit->setText(path);
}

void EinstellungenWidget::onMehrereFahrerChanged(Qt::CheckState state)
{
    const bool mehrere = (state == Qt::Checked);
    m_stdFahrerLabel->setVisible(mehrere);
    m_standardFahrerCb->setVisible(mehrere);

    // Warnung wenn Mehrere Fahrer deaktiviert wird und kein Fahrer vorhanden
    if (!mehrere) {
        const bool keinFahrer = (m_standardFahrerCb->count() <= 1); // nur "(kein)"-Eintrag
        if (keinFahrer) {
            QMessageBox::warning(this,
                tr("Kein Fahrer angelegt"),
                tr("Es ist kein Fahrer angelegt.\n"
                   "Bitte zuerst einen Fahrer anlegen,\n"
                   "bevor \"Mehrere Fahrer\" deaktiviert wird."));
            // Checkbox zurücksetzen
            m_mehrereFahrerCb->setCheckState(Qt::Checked);
        }
    }
}

void EinstellungenWidget::updateWifiWidgetsVisibility(bool visible)
{
    if (m_wifiGroup) m_wifiGroup->setVisible(visible);
}
#endif

#if defined(Q_OS_ANDROID)
#include "einstellungen.moc"
#endif
