#include "fahrtentable.h"
#include "snackbar.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QDateEdit>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QEventLoop>
#include <QDebug>
#include "touchscroll.h"
#include <QTimer>
#include <QMenu>
#include <QApplication>
#include <QPointer>
#include <memory>
#if defined(Q_OS_ANDROID)
#  include <QQuickItem>
#  include <QQmlContext>
#  include "adresstable.h"
#  include "fahrertable.h"
#endif


// Spaltenköpfe über tr() damit sie bei Sprachwechsel korrekt übersetzt werden
static QStringList tableHeaders() {
    return {
        FahrtenTable::tr("Datum"),
        FahrtenTable::tr("Fahrer"),
        FahrtenTable::tr("Startadresse"),
        FahrtenTable::tr("Zieladresse"),
        FahrtenTable::tr("Entfernung (km)"),
        FahrtenTable::tr("Hin & Zurück"),
        FahrtenTable::tr("Bemerkung")
    };
}

// ─── FahrtListBridge ─────────────────────────────────────────────────────────
// Exponiert die Fahrt-Liste als QVariantList an FahrtListView.qml.
// Lifetime: parent = FahrtenTable.
#if defined(Q_OS_ANDROID)
class FahrtListBridge : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList fahrten   READ fahrten   NOTIFY fahrtenChanged)
    Q_PROPERTY(QString  sortField     READ sortField  NOTIFY sortChanged)
    Q_PROPERTY(bool     sortAsc       READ sortAsc    NOTIFY sortChanged)
    Q_PROPERTY(bool     mehrerefahrer READ mehrerefahrer NOTIFY mehrerefahrerChanged)

public:
    explicit FahrtListBridge(QObject *parent = nullptr)
        : QObject(parent) {}

    QVariantList fahrten()      const { return m_fahrten; }
    QString      sortField()    const { return m_sortField; }
    bool         sortAsc()      const { return m_sortAsc; }
    bool         mehrerefahrer() const { return m_mehrerefahrer; }

    void setMehrerefahrer(bool v) {
        if (m_mehrerefahrer != v) {
            m_mehrerefahrer = v;
            emit mehrerefahrerChanged();
        }
    }

    // Wird von FahrtenTable::populateTable() aufgerufen
    void setFahrten(const QVector<Fahrt> &fahrten) {
        m_fahrten.clear();
        for (const Fahrt &f : fahrten) {
            QVariantMap m;
            m["id"]        = f.id;
            m["datum"]     = f.datum.toString("dd.MM.yyyy");
            m["start"]     = f.startAdresse;
            m["ziel"]      = f.zielAdresse;
            m["route"]     = f.startAdresse
                             + (f.zielAdresse.isEmpty() ? "" : " → " + f.zielAdresse);
            m["km"]        = QString::number(f.entfernung, 'f', 1);
            m["hz"]        = f.hinUndZurueck;
            m["bemerkung"] = f.bemerkung;
            m["fahrer"]    = f.fahrerName;
            m_fahrten.append(m);
        }
        emit fahrtenChanged();
    }

signals:
    void fahrtenChanged();
    void sortChanged();
    void mehrerefahrerChanged();
    void addRequested();
    void editRequested(int fahrtId);

public slots:
    Q_INVOKABLE void requestAdd() { emit addRequested(); }

    Q_INVOKABLE void requestEdit(int id) { emit editRequested(id); }

    Q_INVOKABLE void setSort(const QString &field, bool asc) {
        if (m_sortField == field && m_sortAsc == asc) return;
        m_sortField = field;
        m_sortAsc   = asc;
        emit sortChanged();
    }

private:
    QVariantList     m_fahrten;
    QString          m_sortField = "datum";
    bool             m_sortAsc   = false;   // Neuste Fahrt zuerst
    bool             m_mehrerefahrer = false;
};
#endif // Q_OS_ANDROID

// ─── Konstruktor ─────────────────────────────────────────────────────────────

FahrtenTable::FahrtenTable(Database *db, DistanceService *distSvc,
                           SyncManager *syncMgr, QWidget *parent)
    : QWidget(parent), m_db(db), m_distSvc(distSvc), m_syncMgr(syncMgr)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

#if defined(Q_OS_ANDROID)
    // ── Android: QML-ListView ─────────────────────────────────────────────────
    // QTableWidget entfällt komplett; FAB und Sortierung liegen im QML.
    m_listBridge = new FahrtListBridge(this);

    connect(m_listBridge, &FahrtListBridge::addRequested,
            this, &FahrtenTable::onAdd);
    connect(m_listBridge, &FahrtListBridge::editRequested,
            this, [this](int id) {
                Fahrt f = m_db->getFahrtById(id);
                openDialog(&f);
            });
    connect(m_listBridge, &FahrtListBridge::sortChanged,
            this, &FahrtenTable::refresh);

    m_qmlList = new QQuickWidget(this);
    m_qmlList->setAttribute(Qt::WA_AcceptTouchEvents);
    m_qmlList->setResizeMode(QQuickWidget::SizeRootObjectToView);
    m_qmlList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_qmlList->rootContext()->setContextProperty("listBridge", m_listBridge);
    m_qmlList->setSource(QUrl(QStringLiteral("qrc:/FahrtListView.qml")));

    // QML-Fehler sofort in LogCat sichtbar machen
    connect(m_qmlList, &QQuickWidget::statusChanged, this,
            [this](QQuickWidget::Status s) {
                if (s == QQuickWidget::Error)
                    for (const auto &e : m_qmlList->errors())
                        qWarning() << "FahrtListView QML:" << e.toString();
            });

    layout->addWidget(m_qmlList);

    // Dummy-Buttons damit setFahrerVisible() und onDelete() keinen nullptr-Crash
    // erzeugen (werden nie angezeigt)
    m_addBtn  = new QPushButton(this); m_addBtn->hide();
    m_editBtn = new QPushButton(this); m_editBtn->hide();
    m_delBtn  = new QPushButton(this); m_delBtn->hide();

#else
    // ── Desktop: QTableWidget ─────────────────────────────────────────────────
    layout->setContentsMargins(4, 4, 4, 4);

    auto *toolbar = new QHBoxLayout();
    m_addBtn  = new QPushButton(tr("+ Neue Fahrt"));
    m_editBtn = new QPushButton(tr("✎ Bearbeiten"));
    m_delBtn  = new QPushButton(tr("Zeile löschen"));
    toolbar->addWidget(m_addBtn);
    toolbar->addWidget(m_editBtn);
    toolbar->addWidget(m_delBtn);
    toolbar->addStretch();
    layout->addLayout(toolbar);

    m_table = new QTableWidget(0, COL_COUNT, this);
    enableTouchScroll(m_table);
    m_table->setHorizontalHeaderLabels(tableHeaders());
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setColumnWidth(COL_DATUM,  120);
    m_table->setColumnWidth(COL_FAHRER, 150);
    m_table->setColumnWidth(COL_START,  200);
    m_table->setColumnWidth(COL_ZIEL,   200);
    m_table->setColumnWidth(COL_DIST,   130);
    m_table->setColumnWidth(COL_HZ,      90);
    layout->addWidget(m_table);

    m_table->horizontalHeader()->setSortIndicatorShown(true);
    m_table->horizontalHeader()->setSectionsClickable(true);
    connect(m_table->horizontalHeader(), &QHeaderView::sectionClicked, this,
            [this](int col) {
        if (col == COL_HZ) return;
        Qt::SortOrder order =
            (m_lastSortColumn == col && m_lastSortOrder == Qt::AscendingOrder)
            ? Qt::DescendingOrder : Qt::AscendingOrder;
        m_lastSortColumn = col;
        m_lastSortOrder  = order;
        m_table->horizontalHeader()->setSortIndicator(col, order);
        Einstellungen e = m_db->getEinstellungen();
        populateTable(m_db->getAllFahrten(
            e.filterMonat, e.filterJahr,
            colToDbField(col),
            order == Qt::AscendingOrder ? "ASC" : "DESC"));
    });

    connect(m_addBtn,  &QPushButton::clicked, this, &FahrtenTable::onAdd);
    connect(m_editBtn, &QPushButton::clicked, this, &FahrtenTable::onEdit);
    connect(m_delBtn,  &QPushButton::clicked, this, &FahrtenTable::onDelete);
    connect(m_table,   &QTableWidget::doubleClicked, this, [this]{ onEdit(); });
#endif
}

// ─── Hilfsmethoden ───────────────────────────────────────────────────────────

QString FahrtenTable::colToDbField(int col) const
{
    switch (col) {
        case COL_DATUM:  return "datum";
        case COL_FAHRER: return "fahrer_id";
        case COL_START:  return "start_adresse_id";
        case COL_ZIEL:   return "ziel_adresse_id";
        case COL_DIST:   return "entfernung";
        case COL_BEM:    return "bemerkung";
        default:         return "datum";
    }
}

void FahrtenTable::setFahrerVisible(bool visible)
{
#if !defined(Q_OS_ANDROID)
    m_table->setColumnHidden(COL_FAHRER, !visible);
#else
    Q_UNUSED(visible)
#endif
}

void FahrtenTable::refresh()
{
    Einstellungen e = m_db->getEinstellungen();
#if defined(Q_OS_ANDROID)
    populateTable(m_db->getAllFahrten(
        e.filterMonat, e.filterJahr,
        m_listBridge->sortField(),
        m_listBridge->sortAsc() ? "ASC" : "DESC"));
#else
    if (m_lastSortColumn >= 0)
        populateTable(m_db->getAllFahrten(
            e.filterMonat, e.filterJahr,
            colToDbField(m_lastSortColumn),
            m_lastSortOrder == Qt::AscendingOrder ? "ASC" : "DESC"));
    else
        populateTable(m_db->getAllFahrten(e.filterMonat, e.filterJahr));
#endif
}

void FahrtenTable::populateTable(const QVector<Fahrt> &fahrten)
{
#if defined(Q_OS_ANDROID)
    const Einstellungen e = m_db->getEinstellungen();
    m_listBridge->setMehrerefahrer(e.mehrerefahrer);
    m_listBridge->setFahrten(fahrten);
#else
    m_table->setRowCount(0);
    for (const Fahrt &f : fahrten) {
        int row = m_table->rowCount();
        m_table->insertRow(row);
        auto *datItem = new QTableWidgetItem(f.datum.toString("dd.MM.yyyy"));
        datItem->setData(Qt::UserRole, f.id);
        m_table->setItem(row, COL_DATUM,  datItem);
        m_table->setItem(row, COL_FAHRER, new QTableWidgetItem(f.fahrerName));
        m_table->setItem(row, COL_START,  new QTableWidgetItem(f.startAdresse));
        m_table->setItem(row, COL_ZIEL,   new QTableWidgetItem(f.zielAdresse));
        m_table->setItem(row, COL_DIST,
            new QTableWidgetItem(QString::number(f.entfernung, 'f', 1) + " km"));
        m_table->setItem(row, COL_HZ,
            new QTableWidgetItem(f.hinUndZurueck ? tr("Ja") : tr("Nein")));
        m_table->setItem(row, COL_BEM,  new QTableWidgetItem(f.bemerkung));
    }
#endif
}

void FahrtenTable::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
}

// ─── Dialog ──────────────────────────────────────────────────────────────────

void FahrtenTable::openDialog(Fahrt *fahrt)
{
    bool  isNew = (fahrt == nullptr);
    Fahrt f;
    if (!isNew) f = *fahrt;

    QVector<Adresse> adressen = m_db->getAllAdressen();
    QVector<Fahrer>  fahrer   = m_db->getAllFahrer();
    Einstellungen    e        = m_db->getEinstellungen();

    if (isNew) {
        f.datum          = QDate::currentDate();
        f.fahrerId       = e.standardFahrerId;
        f.startAdresseId = e.standardAdresseId;
        f.hinUndZurueck  = e.standardHinZurueck;
    }

#if defined(Q_OS_ANDROID)
    openAndroidQmlDialog(f, isNew, adressen, fahrer, e);
    return;
#endif

    // ── Desktop: Qt-Widgets-Dialog ────────────────────────────────────────────
    auto *dlg = new QDialog();
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle(isNew ? tr("Neue Fahrt") : tr("Fahrt bearbeiten"));
    dlg->setMinimumWidth(440);

    auto *lay = new QFormLayout(dlg);
    lay->setSpacing(10);
    lay->setContentsMargins(16, 16, 16, 16);

    // Datum
    QDate initDate = f.datum.isValid() ? f.datum : QDate::currentDate();
    auto *dateEdit = new QDateEdit(initDate, dlg);
    dateEdit->setCalendarPopup(true);
    dateEdit->setDisplayFormat("dd.MM.yyyy");
    lay->addRow(tr("Datum*:"), dateEdit);

    // Fahrer
    QComboBox *fahrerCb   = nullptr;
    int        fahrerIdFB = f.fahrerId;
    if (e.mehrerefahrer) {
        fahrerCb = new QComboBox();
        fahrerCb->addItem(tr("-- Fahrer wählen --"), 0);
        for (const Fahrer &fa : fahrer) {
            fahrerCb->addItem(fa.isDefault ? tr("unbekannt") : fa.name, fa.id);
            if (fa.id == f.fahrerId) fahrerCb->setCurrentIndex(fahrerCb->count() - 1);
        }
        lay->addRow(tr("Fahrer:"), fahrerCb);
    }

    // Startadresse
    auto *startCb = new QComboBox();
    startCb->addItem(tr("-- Adresse wählen --"), 0);
    for (const Adresse &a : adressen) {
        startCb->addItem(a.anzeige(), a.id);
        if (a.id == f.startAdresseId) startCb->setCurrentIndex(startCb->count() - 1);
    }
    lay->addRow(tr("Startadresse*:"), startCb);

    // Zieladresse
    auto *zielCb = new QComboBox();
    zielCb->addItem(tr("-- Adresse wählen --"), 0);
    for (const Adresse &a : adressen) {
        zielCb->addItem(a.anzeige(), a.id);
        if (a.id == f.zielAdresseId) zielCb->setCurrentIndex(zielCb->count() - 1);
    }
    lay->addRow(tr("Zieladresse*:"), zielCb);

    // Entfernung
    auto *distSpin = new QDoubleSpinBox();
    distSpin->setRange(0, 99999); distSpin->setDecimals(1); distSpin->setSuffix(" km");
    distSpin->setValue(f.entfernung);

    auto *hzCheck = new QCheckBox();
    hzCheck->setChecked(f.hinUndZurueck);

    auto oneWayKm = std::make_shared<double>(
        (f.hinUndZurueck && f.entfernung > 0) ? f.entfernung / 2.0 : f.entfernung);
    QObject::connect(hzCheck, &QCheckBox::toggled, dlg, [distSpin, oneWayKm](bool on) {
        if (*oneWayKm > 0) distSpin->setValue(on ? *oneWayKm * 2.0 : *oneWayKm);
    });

    auto *calcBtn   = new QPushButton(tr("Berechnen"));
    auto *statusLbl = new QLabel();
    statusLbl->setStyleSheet("color:#666;font-size:9pt;");
    statusLbl->setWordWrap(true);

    auto calcConn = std::make_shared<QMetaObject::Connection>();
    QObject::connect(calcBtn, &QPushButton::clicked, dlg,
        [this, calcBtn, statusLbl, startCb, zielCb, distSpin, hzCheck, oneWayKm, calcConn]() {
            int startId = startCb->currentData().toInt();
            int zielId  = zielCb->currentData().toInt();
            if (startId == 0 || zielId == 0 || startId == zielId) {
                statusLbl->setText(tr("Bitte Start- und Zieladresse auswaehlen.")); return;
            }
            Adresse startA = m_db->getAdresseById(startId);
            Adresse zielA  = m_db->getAdresseById(zielId);
            calcBtn->setEnabled(false);
            statusLbl->setText(tr("Berechne..."));
            int reqId = rand();
            if (*calcConn) QObject::disconnect(*calcConn);
            *calcConn = QObject::connect(m_distSvc, &DistanceService::distanceReady,
                calcBtn, [=](int id, double km, bool ok) {
                    if (id != reqId) return;
                    QObject::disconnect(*calcConn);
                    calcBtn->setEnabled(true);
                    if (ok && km > 0) {
                        *oneWayKm = km;
                        distSpin->setValue(hzCheck->isChecked() ? km * 2.0 : km);
                        statusLbl->setText(tr("OK: %1 km (einfache Strecke)").arg(km, 0, 'f', 1));
                    } else {
                        statusLbl->setText(tr("Berechnung fehlgeschlagen."));
                    }
                });
            m_distSvc->requestDistance(reqId, startA, zielA);
        });

    auto *distLay = new QHBoxLayout();
    distLay->addWidget(distSpin, 1);
    distLay->addWidget(calcBtn);
    lay->addRow(tr("Entfernung:"),    distLay);
    lay->addRow(tr("Hin & Zurueck:"), hzCheck);
    lay->addRow(statusLbl);

    auto *bemEdit = new QLineEdit(f.bemerkung);
    lay->addRow(tr("Bemerkung:"), bemEdit);

    auto *hint = new QLabel(tr("* Pflichtfelder"));
    hint->setStyleSheet("color:#888;font-size:9pt;");
    lay->addRow(hint);

    auto *bb = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    bb->button(QDialogButtonBox::Save)->setText(tr("Speichern"));
    lay->addRow(bb);

    auto *saveBtn   = bb->button(QDialogButtonBox::Save);
    auto *cancelBtn = bb->button(QDialogButtonBox::Cancel);
    auto  savedToDB = std::make_shared<bool>(false);

    // Speichern nur aktiv wenn Start + Ziel gewählt
    auto updateSaveBtn = [saveBtn, startCb, zielCb]() {
        saveBtn->setEnabled(startCb->currentData().toInt() != 0
                         && zielCb->currentData().toInt()  != 0);
    };
    updateSaveBtn();
    QObject::connect(startCb, QOverload<int>::of(&QComboBox::currentIndexChanged),
                     dlg, [updateSaveBtn](int) { updateSaveBtn(); });
    QObject::connect(zielCb,  QOverload<int>::of(&QComboBox::currentIndexChanged),
                     dlg, [updateSaveBtn](int) { updateSaveBtn(); });

    QObject::connect(cancelBtn, &QPushButton::clicked, dlg,
        [this, dlg, savedToDB]() {
            if (*savedToDB) { emit dataChanged(); emit savedToDb(); }
            dlg->reject();
        });

    QObject::connect(saveBtn, &QPushButton::clicked, dlg,
        [this, dlg, isNew, f, startCb, zielCb, distSpin, hzCheck,
         bemEdit, fahrerCb, fahrerIdFB, savedToDB, dateEdit]() mutable {
            if (!*savedToDB) {
                f.datum          = dateEdit->date();
                f.fahrerId       = fahrerCb ? fahrerCb->currentData().toInt() : fahrerIdFB;
                f.startAdresseId = startCb->currentData().toInt();
                f.zielAdresseId  = zielCb->currentData().toInt();
                f.entfernung     = distSpin->value();
                f.hinUndZurueck  = hzCheck->isChecked();
                f.bemerkung      = bemEdit->text().trimmed();
                bool ok = isNew ? m_db->insertFahrt(f) : m_db->updateFahrt(f);
                if (!ok) {
                    QMessageBox::warning(this, tr("Fehler"), tr("Fahrt konnte nicht gespeichert werden.")); return;
                }
                *savedToDB = true;
                refresh();
            }
            emit dataChanged(); emit savedToDb();
            dlg->accept();
        });

    dlg->exec();
}


// ─── Slots ───────────────────────────────────────────────────────────────────

void FahrtenTable::onAdd()  { openDialog(nullptr); }

void FahrtenTable::onEdit()
{
#if defined(Q_OS_ANDROID)
    // Android: Edit wird direkt via listBridge->editRequested(id) ausgeloest.
    // Dieser Slot bleibt als Fallback leer.
    Q_UNUSED(this)
#else
    int row = m_table->currentRow();
    if (row < 0) {
        QMessageBox::information(this, tr("Hinweis"), tr("Bitte eine Zeile auswaehlen."));
        return;
    }
    auto *item = m_table->item(row, COL_DATUM);
    if (!item) return;
    Fahrt f = m_db->getFahrtById(item->data(Qt::UserRole).toInt());
    openDialog(&f);
#endif
}

void FahrtenTable::onDelete()
{
#if defined(Q_OS_ANDROID)
    // Android: Loeschen erfolgt im FahrtFormDialog (Bridge::deleteFahrt).
    Q_UNUSED(this)
#else
    int row = m_table->currentRow();
    if (row < 0) {
        QMessageBox::information(this, tr("Hinweis"), tr("Bitte eine Zeile auswaehlen."));
        return;
    }
    auto *item = m_table->item(row, COL_DATUM);
    if (!item) return;
    int id = item->data(Qt::UserRole).toInt();
    if (QMessageBox::question(this, tr("Fahrt loeschen"),
            tr("Diese Fahrt wirklich loeschen?"),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) return;
    if (id > 0 && !m_db->deleteFahrt(id)) {
        QMessageBox::warning(this, tr("Fehler"), tr("Fahrt konnte nicht geloescht werden."));
        return;
    }
    m_table->removeRow(row);
    emit dataChanged();
    emit savedToDb();
#endif
}


// ─── FahrtFormBridge ─────────────────────────────────────────────────────────
// Verbindet das QML-Formular (FahrtFormDialog.qml) mit der C++-Logik.
// adressenModel ist jetzt dynamisch (NOTIFY) damit nach dem Anlegen einer neuen
// Adresse per + -Button das Modell in QML sofort aktualisiert wird.
// Lifetime: parent = QQuickView → wird mit dem Fenster zerstoert.
#if defined(Q_OS_ANDROID)
class FahrtFormBridge : public QObject
{
    Q_OBJECT

    // Konstante Initialdaten
    Q_PROPERTY(bool    isNew          MEMBER m_isNew          CONSTANT)
    Q_PROPERTY(bool    mehrerefahrer  MEMBER m_mehrerefahrer  CONSTANT)
    Q_PROPERTY(QString datumIso       READ   datumIso         NOTIFY datumIsoChanged)
    Q_PROPERTY(int     initFahrerId   READ   initFahrerId     NOTIFY initFahrerIdChanged)
    Q_PROPERTY(int     initStartId    READ   initStartId      NOTIFY initStartIdChanged)
    Q_PROPERTY(int     initZielId     READ   initZielId       NOTIFY initZielIdChanged)
    Q_PROPERTY(double  initEntf       MEMBER m_initEntf       CONSTANT)
    Q_PROPERTY(bool    initHz         MEMBER m_initHz         CONSTANT)
    Q_PROPERTY(QString initBem        MEMBER m_initBem        CONSTANT)
    // Laufzeit-Properties
    Q_PROPERTY(double       oneWayKm      READ oneWayKm      NOTIFY oneWayKmChanged)
    Q_PROPERTY(QVariantList adressenModel READ adressenModel  NOTIFY adressenModelChanged)
    Q_PROPERTY(QVariantList fahrerModel   READ fahrerModel    NOTIFY fahrerModelChanged)

public:
    explicit FahrtFormBridge(
            const Fahrt          &f,
            bool                  isNew,
            bool                  mehrerefahrer,
            const QVector<Adresse>&adressen,
            const QVector<Fahrer> &fahrer,
            Database             *db,
            DistanceService      *distSvc,
            std::function<void()> onSaved,
            std::function<void()> onDeleted,
            QObject              *parent = nullptr)
        : QObject(parent)
        , m_fahrtId(f.id)
        , m_isNew(isNew)
        , m_mehrerefahrer(mehrerefahrer)
        , m_datumIso(f.datum.isValid()
                     ? f.datum.toString(Qt::ISODate)
                     : QDate::currentDate().toString(Qt::ISODate))
        , m_initFahrerId(f.fahrerId)
        , m_fallbackFahrerId(f.fahrerId > 0
                             ? f.fahrerId
                             : db->getDefaultFahrerId())
        , m_initStartId(f.startAdresseId)
        , m_initZielId(f.zielAdresseId)
        , m_initEntf(f.entfernung)
        , m_initHz(f.hinUndZurueck)
        , m_initBem(f.bemerkung)
        , m_oneWayKm((f.hinUndZurueck && f.entfernung > 0)
                     ? f.entfernung / 2.0 : f.entfernung)
        , m_db(db)
        , m_distSvc(distSvc)
        , m_onSaved(std::move(onSaved))
        , m_onDeleted(std::move(onDeleted))
    {
        buildAdressenModel(adressen);

        if (mehrerefahrer) {
            QVariantMap ph; ph[QStringLiteral("id")] = 0;
            ph[QStringLiteral("text")] = tr("-- Fahrer waehlen --");
            m_fahrerModel.append(ph);
            for (const Fahrer &fa : fahrer) {
                QVariantMap item;
                item[QStringLiteral("id")]   = fa.id;
                item[QStringLiteral("text")] = fa.isDefault ? tr("unbekannt") : fa.name;
                m_fahrerModel.append(item);
            }
        }
    }

    QString      datumIso()        const { return m_datumIso;            }
    double       oneWayKm()        const { return m_oneWayKm;            }
    QVariantList adressenModel()   const { return m_adressenModel;       }
    QVariantList fahrerModel()     const { return m_fahrerModel;         }
    int          initStartId()     const { return m_initStartId;         }
    int          initZielId()      const { return m_initZielId;          }
    int          initFahrerId()    const { return m_initFahrerId;        }

signals:
    void datumIsoChanged();
    void oneWayKmChanged();
    void adressenModelChanged();
    void fahrerModelChanged();
    void initStartIdChanged();
    void initZielIdChanged();
    void initFahrerIdChanged();
    // forField: 1 = Startadresse, 2 = Zieladresse
    void newAdressCreated(int id, int forField);
    void newFahrerCreated(int id);
    void distanceStatus(QString text, bool calculating);
    void closeRequested();
    void snackbarRequested(QString text, int ms);

public slots:
    Q_INVOKABLE void save(const QString &datumIso, int fahrerId,
                          int startId, int zielId,
                          double km, bool hz, const QString &bemerkung)
    {
        if (startId == 0) {
            emit snackbarRequested(tr("Bitte Startadresse auswaehlen."), 3000); return;
        }
        if (zielId == 0) {
            emit snackbarRequested(tr("Bitte Zieladresse auswaehlen."), 3000); return;
        }
        QDate d = QDate::fromString(datumIso, Qt::ISODate);
        if (!d.isValid()) {
            emit snackbarRequested(tr("Ungueltiges Datum."), 3000); return;
        }

        Fahrt f;
        f.id             = m_fahrtId;
        f.datum          = d;
        f.fahrerId       = m_mehrerefahrer ? fahrerId : m_fallbackFahrerId;
        if (f.fahrerId == 0) {
            // Nur im Mehrfahrer-Modus kann fahrerId 0 sein (keiner ausgewählt)
            emit snackbarRequested(tr("Bitte einen Fahrer auswaehlen."), 3000);
            return;
        }
        f.startAdresseId = startId;
        f.zielAdresseId  = zielId;
        f.entfernung     = km;
        f.hinUndZurueck  = hz;
        f.bemerkung      = bemerkung;

        bool ok = m_isNew ? m_db->insertFahrt(f) : m_db->updateFahrt(f);
        if (!ok) {
            emit snackbarRequested(tr("Fahrt konnte nicht gespeichert werden."), 3000); return;
        }
        m_onSaved();
        emit snackbarRequested(
            m_isNew ? tr("Fahrt gespeichert") : tr("Fahrt aktualisiert"), 2500);
        emit closeRequested();
    }

    Q_INVOKABLE void cancel()     { emit closeRequested(); }

    Q_INVOKABLE void deleteFahrt()
    {
        if (m_fahrtId > 0 && !m_db->deleteFahrt(m_fahrtId)) {
            emit snackbarRequested(tr("Fahrt konnte nicht geloescht werden."), 3000); return;
        }
        m_onDeleted();
        emit snackbarRequested(tr("Fahrt geloescht"), 2000);
        emit closeRequested();
    }

    Q_INVOKABLE void calcDistance(int startId, int zielId)
    {
        if (startId == 0 || zielId == 0 || startId == zielId) {
            emit distanceStatus(tr("Bitte Start- und Zieladresse auswaehlen."), false); return;
        }
        Adresse startA = m_db->getAdresseById(startId);
        Adresse zielA  = m_db->getAdresseById(zielId);
        int reqId = rand();
        if (m_calcConn) QObject::disconnect(m_calcConn);
        emit distanceStatus(tr("Berechne..."), true);
        m_calcConn = QObject::connect(m_distSvc, &DistanceService::distanceReady, this,
            [this, reqId](int id, double km, bool ok) {
                if (id != reqId) return;
                QObject::disconnect(m_calcConn);
                if (ok && km > 0) {
                    m_oneWayKm = km;
                    emit oneWayKmChanged();
                    emit distanceStatus(
                        tr("OK: %1 km (einfache Strecke)").arg(km, 0, 'f', 1), false);
                } else {
                    emit distanceStatus(tr("Berechnung fehlgeschlagen."), false);
                }
            });
        m_distSvc->requestDistance(reqId, startA, zielA);
    }

    Q_INVOKABLE void openCalendar()
    {
        QDate cur = QDate::fromString(m_datumIso, Qt::ISODate);
        if (!cur.isValid()) cur = QDate::currentDate();
        // parent() == m_qmlList (QQuickWidget) – Source-Swap statt QQuickView
        auto *qw = qobject_cast<QQuickWidget*>(parent());
        FahrtenTable::showQmlDatePicker(qw, cur, [this](QDate d) {
            m_datumIso = d.toString(Qt::ISODate);
            emit datumIsoChanged();
        });
    }

    // Öffnet AdressFormDialog.qml; nach Speichern wird adressenModel neu geladen
    // und newAdressCreated(id, forField) gefeuert → QML-ComboBox wählt neue Adresse.
    Q_INVOKABLE void requestNewAdress(int forField)
    {
        auto *qw = qobject_cast<QQuickWidget*>(parent());
        if (!qw) return;

        auto snackFn = [qw](const QString &text, int ms) {
            Snackbar::show(qw, text, ms);
        };

        auto *fahrtBridge = this;

        AdressTable::runAdressQmlView(
            nullptr, /*isNew=*/true, m_db, qw,
            [fahrtBridge, forField](int newId) {
                fahrtBridge->reloadAdressen(newId, forField);
            },
            /*onDeleted=*/{},
            std::move(snackFn),
            qw,
            [qw, fahrtBridge]() {
                qw->rootContext()->setContextProperty(
                    QStringLiteral("bridge"), fahrtBridge);
                QTimer::singleShot(0, qw, [qw]() {
                    if (qw->parentWidget())
                        qw->setGeometry(0, 0,
                            qw->parentWidget()->width(),
                            qw->parentWidget()->height());
                    qw->setSource(QUrl(QStringLiteral("qrc:/FahrtFormDialog.qml")));
                });
            });
    }

    Q_INVOKABLE void requestNewFahrer()
    {
        auto *qw = qobject_cast<QQuickWidget*>(parent());
        if (!qw) return;

        auto snackFn = [qw](const QString &text, int ms) {
            Snackbar::show(qw, text, ms);
        };

        auto *fahrtBridge = this;

        FahrerTable::runFahrerQmlView(
            m_db, qw,
            [fahrtBridge](int newId) {
                fahrtBridge->reloadFahrer(newId);
            },
            std::move(snackFn),
            [qw, fahrtBridge]() {
                qw->rootContext()->setContextProperty(
                    QStringLiteral("bridge"), fahrtBridge);
                QTimer::singleShot(0, qw, [qw]() {
                    if (qw->parentWidget())
                        qw->setGeometry(0, 0,
                            qw->parentWidget()->width(),
                            qw->parentWidget()->height());
                    qw->setSource(QUrl(QStringLiteral("qrc:/FahrtFormDialog.qml")));
                });
            });
    }

private:
    void buildFahrerModel(const QVector<Fahrer> &fahrer)
    {
        m_fahrerModel.clear();
        QVariantMap ph;
        ph[QStringLiteral("id")]   = 0;
        ph[QStringLiteral("text")] = tr("-- Fahrer waehlen --");
        m_fahrerModel.append(ph);
        for (const Fahrer &fa : fahrer) {
            QVariantMap item;
            item[QStringLiteral("id")]   = fa.id;
            item[QStringLiteral("text")] = fa.isDefault ? tr("unbekannt") : fa.name;
            m_fahrerModel.append(item);
        }
    }

    void reloadFahrer(int newId)
    {
        buildFahrerModel(m_db->getAllFahrer());
        emit fahrerModelChanged();
        if (newId > 0) {
            m_initFahrerId = newId;
            emit initFahrerIdChanged();
            emit newFahrerCreated(newId);
        }
    }

    void buildAdressenModel(const QVector<Adresse> &adressen)
    {
        m_adressenModel.clear();
        QVariantMap ph;
        ph[QStringLiteral("id")]   = 0;
        ph[QStringLiteral("text")] = tr("-- Adresse waehlen --");
        m_adressenModel.append(ph);
        for (const Adresse &a : adressen) {
            QVariantMap item;
            item[QStringLiteral("id")]   = a.id;
            item[QStringLiteral("text")] = a.anzeige();
            m_adressenModel.append(item);
        }
    }

    void reloadAdressen(int lastCreatedId, int forField)
    {
        buildAdressenModel(m_db->getAllAdressen());
        emit adressenModelChanged();
        if (lastCreatedId > 0) {
            // initStartId/initZielId aktualisieren damit nach Source-Swap
            // die neue Adresse korrekt vorausgewählt wird.
            if (forField == 1) { m_initStartId = lastCreatedId; emit initStartIdChanged(); }
            if (forField == 2) { m_initZielId  = lastCreatedId; emit initZielIdChanged();  }
            emit newAdressCreated(lastCreatedId, forField);
        }
    }

    int      m_fahrtId          = 0;
    bool     m_isNew;
    bool     m_mehrerefahrer;
    QString  m_datumIso;
    int      m_initFahrerId     = 0;
    int      m_fallbackFahrerId = 0;
    int      m_initStartId      = 0;
    int      m_initZielId       = 0;
    double   m_initEntf         = 0.0;
    bool     m_initHz           = false;
    QString  m_initBem;
    double   m_oneWayKm         = 0.0;
    QVariantList m_adressenModel;
    QVariantList m_fahrerModel;
    Database        *m_db;
    DistanceService *m_distSvc;
    std::function<void()> m_onSaved;
    std::function<void()> m_onDeleted;
    QMetaObject::Connection m_calcConn;
};
#endif // Q_OS_ANDROID

// ─── DatePickerBridge ────────────────────────────────────────────────────────
// Qt 6.8 / Android 16: kein QQuickView mehr – Source-Swap auf QQuickWidget.
// closeRequested → Aufrufer (showQmlDatePicker) swappt zurück zu FahrtFormDialog.
#if defined(Q_OS_ANDROID)
class DatePickerBridge : public QObject
{
    Q_OBJECT
public:
    explicit DatePickerBridge(std::function<void(QDate)> cb, QObject *parent)
        : QObject(parent), m_cb(std::move(cb)) {}

signals:
    void closeRequested();

public slots:
    void onDateAccepted(const QString &iso)
    {
        QDate d = QDate::fromString(iso, Qt::ISODate);
        if (d.isValid()) m_cb(d);
        emit closeRequested();
    }
    void onCancelled()
    {
        emit closeRequested();
    }

private:
    std::function<void(QDate)> m_cb;
};
#endif // Q_OS_ANDROID

// ─── openAndroidQmlDialog ────────────────────────────────────────────────────
// Qt 6.8 / Android 16: QQuickView neben QQuickWidget → EGL-Konflikt → SIGABRT.
// Lösung: m_qmlList direkt wiederverwenden, Source tauschen statt neues Fenster.
#if defined(Q_OS_ANDROID)
void FahrtenTable::openAndroidQmlDialog(const Fahrt           &f,
                                        bool                   isNew,
                                        const QVector<Adresse>&adressen,
                                        const QVector<Fahrer> &fahrer,
                                        const Einstellungen   &e)
{
    if (!m_qmlList) return;

    auto onSaved = [this]() {
        emit dataChanged();
        emit savedToDb();
    };

    auto *bridge = new FahrtFormBridge(
        f, isNew, e.mehrerefahrer,
        adressen, fahrer,
        m_db, m_distSvc,
        onSaved, onSaved,
        m_qmlList);   // parent = m_qmlList → Bridge-Lifetime an Widget gebunden

    m_qmlList->rootContext()->setContextProperty("bridge", bridge);

    QObject::connect(bridge, &FahrtFormBridge::snackbarRequested,
        this, [this](const QString &text, int ms) {
            Snackbar::show(m_qmlList, text, ms);
        });

    // Zurück zur Listenansicht wenn Formular geschlossen wird.
    // QTimer::singleShot(0) ist zwingend: setSource() darf nicht synchron
    // aus einem QML-Signal-Handler heraus aufgerufen werden (SIGABRT).
    QObject::connect(bridge, &FahrtFormBridge::closeRequested,
        this, [this, bridge]() {
            bridge->deleteLater();
            m_qmlList->rootContext()->setContextProperty("bridge", QVariant());
            m_qmlList->rootContext()->setContextProperty("listBridge", m_listBridge);
            QTimer::singleShot(0, this, [this]() {
                m_qmlList->setSource(QUrl(QStringLiteral("qrc:/FahrtListView.qml")));
                refresh();
            });
        });

    // Source auf Formular wechseln – gleicher EGL-Context, kein Konflikt
    QTimer::singleShot(0, this, [this]() {
        m_qmlList->setSource(QUrl(QStringLiteral("qrc:/FahrtFormDialog.qml")));
    });
}
#endif // Q_OS_ANDROID

// ─── showQmlDatePicker ───────────────────────────────────────────────────────
// Qt 6.8 / Android 16: Source-Swap auf dem übergebenen QQuickWidget.
// FahrtFormDialog → DatePickerDialog → FahrtFormDialog (gleicher EGL-Context).
#if defined(Q_OS_ANDROID)
void FahrtenTable::showQmlDatePicker(QQuickWidget *qw,
                                     QDate         current,
                                     std::function<void(QDate)> onAccepted)
{
    if (!qw) return;

    auto *bridge = new DatePickerBridge(onAccepted, qw);

    qw->rootContext()->setContextProperty(
        QStringLiteral("initialIsoDate"), current.toString(Qt::ISODate));
    qw->rootContext()->setContextProperty(
        QStringLiteral("datePicker"), bridge);

    // Nach Accepted/Cancelled: zurück zu FahrtFormDialog.
    // QTimer::singleShot(0) – nicht synchron aus QML-Signal-Handler.
    QObject::connect(bridge, &DatePickerBridge::closeRequested,
        qw, [qw, bridge]() {
            bridge->deleteLater();
            qw->rootContext()->setContextProperty(
                QStringLiteral("datePicker"), QVariant());
            QTimer::singleShot(0, qw, [qw]() {
                qw->setSource(QUrl(QStringLiteral("qrc:/FahrtFormDialog.qml")));
            });
        });

    QObject::connect(qw, &QQuickWidget::statusChanged, qw,
        [qw, bridge](QQuickWidget::Status s) {
            if (s == QQuickWidget::Error) {
                for (const auto &e : qw->errors())
                    qWarning() << "DatePickerDialog QML:" << e.toString();
                return;
            }
            if (s != QQuickWidget::Ready) return;
            // Sofort trennen: beim Zurück-Swap zu FahrtFormDialog feuert
            // statusChanged erneut – bridge ist dann bereits deleteLater'd.
            QObject::disconnect(qw, &QQuickWidget::statusChanged, qw, nullptr);
            auto *root = qw->rootObject();
            if (!root) return;
            QObject::connect(root,   SIGNAL(dateAccepted(QString)),
                             bridge, SLOT(onDateAccepted(QString)),
                             Qt::UniqueConnection);
            QObject::connect(root,   SIGNAL(cancelled()),
                             bridge, SLOT(onCancelled()),
                             Qt::UniqueConnection);
        });

    QTimer::singleShot(0, qw, [qw]() {
        qw->setSource(QUrl(QStringLiteral("qrc:/DatePickerDialog.qml")));
    });
}
#endif // Q_OS_ANDROID

// MOC verarbeitet alle Q_OBJECT-Klassen die in diesem .cpp definiert sind.
// Muss die allerletzte Zeile der Datei sein.
#if defined(Q_OS_ANDROID)
#include "fahrtentable.moc"
#endif
