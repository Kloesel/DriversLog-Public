#include "adresstable.h"
#include "snackbar.h"
#include "touchscroll.h"
#include <QMenu>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QDialog>
#include <QPointer>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QLabel>
#include <QScrollArea>
#include <QScrollBar>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QApplication>
#include <QPushButton>
#include <algorithm>
#include <memory>
#include <QSet>

#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
#  include <QStringConverter>
#else
#  include <QTextCodec>
#endif

#if defined(Q_OS_ANDROID)
#  include <QQuickView>
#  include <QQuickItem>
#  include <QQmlContext>
#endif

// ─── Android: AdressListBridge ───────────────────────────────────────────────
// Exponiert die Adress-Liste als QVariantList an AdressListView.qml.
// Sortierung erfolgt client-seitig (da getAllAdressen() keine ORDER-Option hat).
#if defined(Q_OS_ANDROID)
class AdressListBridge : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList adressen  READ adressen  NOTIFY adressenChanged)
    Q_PROPERTY(QString  sortField     READ sortField  NOTIFY sortChanged)
    Q_PROPERTY(bool     sortAsc       READ sortAsc    NOTIFY sortChanged)

public:
    explicit AdressListBridge(QObject *parent = nullptr)
        : QObject(parent) {}

    QVariantList adressen()  const { return m_adressen; }
    QString      sortField() const { return m_sortField; }
    bool         sortAsc()   const { return m_sortAsc;   }

    void setAdressen(const QVector<Adresse> &list) {
        // Client-seitige Sortierung
        QVector<Adresse> sorted = list;
        std::sort(sorted.begin(), sorted.end(), [this](const Adresse &a, const Adresse &b) {
            QString ka, kb, ka2, kb2;
            if (m_sortField == QLatin1String("ort")) {
                ka = a.ort.toLower();           kb = b.ort.toLower();
                ka2 = a.bezeichnung.toLower();  kb2 = b.bezeichnung.toLower();
            } else {
                ka = a.bezeichnung.toLower();   kb = b.bezeichnung.toLower();
                ka2 = a.ort.toLower();          kb2 = b.ort.toLower();
            }
            if (ka != kb) return m_sortAsc ? (ka < kb) : (ka > kb);
            return m_sortAsc ? (ka2 < kb2) : (ka2 > kb2);
        });

        m_adressen.clear();
        for (const Adresse &a : sorted) {
            QVariantMap m;
            m[QStringLiteral("id")] = a.id;
            m[QStringLiteral("bezeichnung")] =
                a.bezeichnung.isEmpty() ? a.ort : a.bezeichnung;

            // Kombinierte Adresszeile: "Straße HNr, PLZ Ort"
            QStringList parts;
            QString sh = a.strasse;
            if (!a.hausnummer.isEmpty()) sh += QLatin1Char(' ') + a.hausnummer;
            if (!sh.trimmed().isEmpty()) parts << sh.trimmed();
            QString po;
            if (!a.plz.isEmpty()) po = a.plz;
            if (!a.ort.isEmpty())
                po += (po.isEmpty() ? QString() : QStringLiteral(" ")) + a.ort;
            if (!po.trimmed().isEmpty()) parts << po.trimmed();
            m[QStringLiteral("adresse")] = parts.join(QStringLiteral(", "));

            m_adressen.append(m);
        }
        emit adressenChanged();
    }

signals:
    void adressenChanged();
    void sortChanged();
    void addRequested();
    void editRequested(int id);

public slots:
    Q_INVOKABLE void requestAdd()         { emit addRequested(); }
    Q_INVOKABLE void requestEdit(int id)  { emit editRequested(id); }
    Q_INVOKABLE void setSort(const QString &field, bool asc) {
        if (m_sortField == field && m_sortAsc == asc) return;
        m_sortField = field;
        m_sortAsc   = asc;
        emit sortChanged();
    }

private:
    QVariantList m_adressen;
    QString      m_sortField = QStringLiteral("bezeichnung");
    bool         m_sortAsc   = true;
};

// ─── Android: AdressFormBridge ───────────────────────────────────────────────
// Verbindet AdressFormDialog.qml mit der C++-Logik.
class AdressFormBridge : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool    isNew     MEMBER m_isNew     CONSTANT)
    Q_PROPERTY(QString initBez   MEMBER m_initBez   CONSTANT)
    Q_PROPERTY(QString initStr   MEMBER m_initStr   CONSTANT)
    Q_PROPERTY(QString initHnr   MEMBER m_initHnr   CONSTANT)
    Q_PROPERTY(QString initPlz   MEMBER m_initPlz   CONSTANT)
    Q_PROPERTY(QString initOrt   MEMBER m_initOrt   CONSTANT)
    Q_PROPERTY(QString errorText READ errorText NOTIFY errorTextChanged)

public:
    explicit AdressFormBridge(
            const Adresse            &a,
            bool                      isNew,
            Database                 *db,
            std::function<void(int)>  onSaved,
            std::function<void()>     onDeleted,
            QObject                  *parent = nullptr)
        : QObject(parent)
        , m_adresseId(a.id)
        , m_isNew(isNew)
        , m_initBez(a.bezeichnung)
        , m_initStr(a.strasse)
        , m_initHnr(a.hausnummer)
        , m_initPlz(a.plz)
        , m_initOrt(a.ort)
        , m_db(db)
        , m_onSaved(std::move(onSaved))
        , m_onDeleted(std::move(onDeleted))
    {}

    QString errorText() const { return m_errorText; }

signals:
    void closeRequested();
    void snackbarRequested(QString text, int ms);
    void errorTextChanged();

public slots:
    Q_INVOKABLE void save(const QString &bez, const QString &str,
                          const QString &hnr, const QString &plz,
                          const QString &ort)
    {
        if (ort.trimmed().isEmpty()) {
            emit snackbarRequested(tr("Ort ist ein Pflichtfeld."), 3000);
            return;
        }
        Adresse a;
        a.id          = m_adresseId;
        a.bezeichnung = bez.trimmed();
        a.strasse     = str.trimmed();
        a.hausnummer  = hnr.trimmed();
        a.plz         = plz.trimmed();
        a.ort         = ort.trimmed();
        a.land        = QStringLiteral("Deutschland");

        if (!a.strasse.isEmpty() || !a.plz.isEmpty()) {
            int dupId = m_db->findDuplicateAdresseId(a.strasse, a.hausnummer, a.plz, a.ort,
                                                      m_isNew ? 0 : a.id);
            if (dupId > 0) {
                m_errorText = tr("Diese Adresse ist bereits vorhanden.");
                emit errorTextChanged();
                return;
            }
        }
        m_errorText.clear();
        emit errorTextChanged();

        bool ok = m_isNew ? m_db->insertAdresse(a) : m_db->updateAdresse(a);
        if (!ok) {
            emit snackbarRequested(tr("Adresse konnte nicht gespeichert werden."), 3000);
            return;
        }
        if (m_onSaved) m_onSaved(a.id);
        emit snackbarRequested(
            m_isNew ? tr("Adresse gespeichert") : tr("Adresse aktualisiert"), 2500);
        emit closeRequested();
    }

    Q_INVOKABLE void deleteAdresse()
    {
        if (m_adresseId > 0 && !m_db->deleteAdresse(m_adresseId)) {
            emit snackbarRequested(tr("Adresse konnte nicht geloescht werden."), 3000);
            return;
        }
        if (m_onDeleted) m_onDeleted();
        emit snackbarRequested(tr("Adresse geloescht"), 2000);
        emit closeRequested();
    }

    Q_INVOKABLE void cancel() { emit closeRequested(); }

private:
    int      m_adresseId = 0;
    bool     m_isNew;
    QString  m_initBez, m_initStr, m_initHnr, m_initPlz, m_initOrt;
    QString  m_errorText;
    Database *m_db;
    std::function<void(int)> m_onSaved;
    std::function<void()>    m_onDeleted;
};
#endif // Q_OS_ANDROID

// ─── Konstruktor ─────────────────────────────────────────────────────────────

AdressTable::AdressTable(Database *db, SyncManager *syncMgr, QWidget *parent)
    : QWidget(parent), m_db(db), m_syncMgr(syncMgr)
{
    auto *layout = new QVBoxLayout(this);

#if defined(Q_OS_ANDROID)
    // ── Android: QML-ListView ─────────────────────────────────────────────────
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_listBridge = new AdressListBridge(this);

    connect(m_listBridge, &AdressListBridge::addRequested,
            this, &AdressTable::onAdd);
    connect(m_listBridge, &AdressListBridge::editRequested,
            this, [this](int id) {
                Adresse a = m_db->getAdresseById(id);
                openDialog(&a);
            });
    connect(m_listBridge, &AdressListBridge::sortChanged,
            this, &AdressTable::refresh);

    m_qmlList = new QQuickWidget(this);
    m_qmlList->setAttribute(Qt::WA_AcceptTouchEvents);
    m_qmlList->setResizeMode(QQuickWidget::SizeRootObjectToView);
    m_qmlList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_qmlList->rootContext()->setContextProperty(
        QStringLiteral("listBridge"), m_listBridge);
    m_qmlList->setSource(QUrl(QStringLiteral("qrc:/AdressListView.qml")));

    connect(m_qmlList, &QQuickWidget::statusChanged, this,
            [this](QQuickWidget::Status s) {
                if (s == QQuickWidget::Error)
                    for (const auto &e : m_qmlList->errors())
                        qWarning() << "AdressListView QML:" << e.toString();
            });

    layout->addWidget(m_qmlList);

    // Dummy-Buttons (werden nie angezeigt, verhindert nullptr-Crashes)
    m_addBtn    = new QPushButton(this); m_addBtn->hide();
    m_editBtn   = new QPushButton(this); m_editBtn->hide();
    m_delBtn    = new QPushButton(this); m_delBtn->hide();
    m_importBtn = new QPushButton(this); m_importBtn->hide();

#else
    // ── Desktop: QTableWidget ─────────────────────────────────────────────────
    layout->setContentsMargins(4, 4, 4, 4);

    auto *toolbar = new QHBoxLayout();
    m_addBtn    = new QPushButton(tr("+ Neue Adresse"));
    m_editBtn   = new QPushButton(tr("✎ Bearbeiten"));
    m_delBtn    = new QPushButton(tr("Löschen"));
    m_importBtn = new QPushButton(tr("📂 CSV importieren"));
    toolbar->addWidget(m_addBtn);
    toolbar->addWidget(m_editBtn);
    toolbar->addWidget(m_delBtn);
    toolbar->addWidget(m_importBtn);
    toolbar->addStretch();
    layout->addLayout(toolbar);

    m_table = new QTableWidget(0, C_COUNT, this);
    enableTouchScroll(m_table);
    m_table->setHorizontalHeaderLabels(
        {tr("Bezeichnung"), tr("Straße"), tr("Hausnr."), tr("PLZ"), tr("Ort")});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setColumnWidth(C_BEZ, 180);
    m_table->setColumnWidth(C_STR, 180);
    m_table->setColumnWidth(C_HNR,  70);
    m_table->setColumnWidth(C_PLZ,  70);
    layout->addWidget(m_table);

    m_table->horizontalHeader()->setSortIndicatorShown(true);
    connect(m_table->horizontalHeader(), &QHeaderView::sectionClicked, this,
            [this](int col) {
        Qt::SortOrder order =
            (m_lastSortColumn == col && m_lastSortOrder == Qt::AscendingOrder)
            ? Qt::DescendingOrder : Qt::AscendingOrder;
        m_table->setSortingEnabled(true);
        m_table->sortItems(col, order);
        m_table->setSortingEnabled(false);
        m_table->horizontalHeader()->setSortIndicator(col, order);
        m_lastSortColumn = col;
        m_lastSortOrder  = order;
    });

    connect(m_addBtn,    &QPushButton::clicked, this, &AdressTable::onAdd);
    connect(m_editBtn,   &QPushButton::clicked, this, &AdressTable::onEdit);
    connect(m_delBtn,    &QPushButton::clicked, this, &AdressTable::onDelete);
    connect(m_importBtn, &QPushButton::clicked, this, &AdressTable::onImportCsv);

    // ── Doppelklick-Fix: Index wird korrekt an onEdit() weitergegeben ─────────
    connect(m_table, &QTableWidget::doubleClicked, this,
            [this](const QModelIndex &idx) {
                if (!idx.isValid()) return;
                m_table->selectRow(idx.row());
                onEdit();
            });
#endif
}

// ─── Hilfsmethoden ───────────────────────────────────────────────────────────

void AdressTable::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
}

void AdressTable::refresh()
{
    populateTable(m_db->getAllAdressen());
}

void AdressTable::populateTable(const QVector<Adresse> &list)
{
#if defined(Q_OS_ANDROID)
    m_listBridge->setAdressen(list);
#else
    m_table->setSortingEnabled(false);
    m_table->setRowCount(0);
    for (const Adresse &a : list) {
        int row = m_table->rowCount();
        m_table->insertRow(row);
        auto *bez = new QTableWidgetItem(a.bezeichnung);
        bez->setData(Qt::UserRole, a.id);
        m_table->setItem(row, C_BEZ, bez);
        m_table->setItem(row, C_STR, new QTableWidgetItem(a.strasse));
        m_table->setItem(row, C_HNR, new QTableWidgetItem(a.hausnummer));
        m_table->setItem(row, C_PLZ, new QTableWidgetItem(a.plz));
        m_table->setItem(row, C_ORT, new QTableWidgetItem(a.ort));
    }
    m_table->setSortingEnabled(false);
#endif
}

// ─── openDialog (Desktop) / openDialog→QML (Android) ─────────────────────────

void AdressTable::openDialog(Adresse *adresse)
{
#if defined(Q_OS_ANDROID)
    openAndroidQmlDialog(adresse);
#else
    bool    isNew = (adresse == nullptr);
    Adresse a;
    if (!isNew) a = *adresse;

    auto *dlg = new QDialog();
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle(isNew ? tr("Neue Adresse") : tr("Adresse bearbeiten"));
    dlg->setMinimumWidth(380);

    auto *lay = new QFormLayout(dlg);
    lay->setSpacing(10);
    lay->setContentsMargins(16, 16, 16, 16);

    auto *bezEdit = new QLineEdit(a.bezeichnung);
    auto *strEdit = new QLineEdit(a.strasse);
    auto *hnrEdit = new QLineEdit(a.hausnummer);
    auto *plzEdit = new QLineEdit(a.plz);
    auto *ortEdit = new QLineEdit(a.ort);

    bezEdit->setPlaceholderText(tr("z.B. Büro, Zuhause"));
    ortEdit->setPlaceholderText(tr("Pflichtfeld"));

    lay->addRow(tr("Bezeichnung:"), bezEdit);
    lay->addRow(tr("Straße:"),      strEdit);
    lay->addRow(tr("Hausnr.:"),     hnrEdit);
    lay->addRow(tr("PLZ:"),         plzEdit);
    lay->addRow(tr("Ort*:"),        ortEdit);

    auto *hint = new QLabel(tr("* Pflichtfeld"));
    hint->setStyleSheet(QStringLiteral("color: #888; font-size: 9pt;"));
    lay->addRow(hint);

    auto *syncLbl = new QLabel(QString(), dlg);
    syncLbl->setWordWrap(true);
    syncLbl->setStyleSheet(QStringLiteral("color: #2980b9; font-size: 11pt;"));
    lay->addRow(syncLbl);

    auto *bb = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    bb->button(QDialogButtonBox::Save)->setText(tr("Speichern"));
    lay->addRow(bb);

    auto *saveBtn   = bb->button(QDialogButtonBox::Save);
    auto *cancelBtn = bb->button(QDialogButtonBox::Cancel);
    auto  savedToDB = std::make_shared<bool>(false);

    // Speichern nur aktiv wenn Ort gefüllt
    saveBtn->setEnabled(!a.ort.isEmpty());
    QObject::connect(ortEdit, &QLineEdit::textChanged, saveBtn,
        [saveBtn](const QString &t) { saveBtn->setEnabled(!t.trimmed().isEmpty()); });

    QObject::connect(cancelBtn, &QPushButton::clicked, dlg,
        [this, dlg, savedToDB]() {
            if (*savedToDB) { emit dataChanged(); emit savedToDb(); }
            dlg->reject();
        });

    QObject::connect(saveBtn, &QPushButton::clicked, dlg,
        [this, dlg, isNew, a,
         bezEdit, strEdit, hnrEdit, plzEdit, ortEdit, savedToDB]() mutable {
            if (!*savedToDB) {
                a.bezeichnung = bezEdit->text().trimmed();
                a.strasse     = strEdit->text().trimmed();
                a.hausnummer  = hnrEdit->text().trimmed();
                a.plz         = plzEdit->text().trimmed();
                a.ort         = ortEdit->text().trimmed();
                a.land        = QStringLiteral("Deutschland");

                int dupId = m_db->findDuplicateAdresseId(
                    a.strasse, a.hausnummer, a.plz, a.ort, isNew ? 0 : a.id);
                if (dupId > 0 && (!a.strasse.isEmpty() || !a.plz.isEmpty())) {
                    QMessageBox::warning(dlg, tr("Hinweis"),
                        tr("Diese Adresse ist bereits vorhanden."));
                    return;
                }

                bool ok = isNew ? m_db->insertAdresse(a) : m_db->updateAdresse(a);
                if (!ok) {
                    QMessageBox::warning(this, tr("Fehler"),
                        tr("Adresse konnte nicht gespeichert werden."));
                    return;
                }
                *savedToDB = true;
                refresh();
            }
            emit dataChanged();
            emit savedToDb();
            dlg->accept();
        });

    dlg->exec();
#endif
}

// ─── Android: QML-Dialog (Adresse anlegen / bearbeiten) ─────────────────────
#if defined(Q_OS_ANDROID)

/*static*/ void AdressTable::runAdressQmlView(
        Adresse  *adresse,
        bool      isNew,
        Database *db,
        QWidget  * /*parentWidget*/,
        std::function<void(int)>             onSaved,
        std::function<void()>                onDeleted,
        std::function<void(const QString&, int)> showSnackbar,
        QQuickWidget *qmlList,
        std::function<void()> onClose)
{
    // Qt 6.8 / Android 16: Source-Swap statt QQuickView (kein EGL-Konflikt)
    if (!qmlList) return;

    Adresse a;
    if (!isNew && adresse) a = *adresse;

    auto *bridge = new AdressFormBridge(
        a, isNew, db,
        std::move(onSaved),
        std::move(onDeleted),
        qmlList);

    qmlList->rootContext()->setContextProperty(QStringLiteral("bridge"), bridge);

    QObject::connect(bridge, &AdressFormBridge::snackbarRequested,
        bridge, [showSnackbar](const QString &text, int ms) {
            if (showSnackbar) showSnackbar(text, ms);
        });

    // Zurück zur vorherigen Ansicht. onClose überschreibt Standard (AdressListView).
    QObject::connect(bridge, &AdressFormBridge::closeRequested,
        qmlList, [qmlList, bridge, onClose] {
            qmlList->rootContext()->setContextProperty(QStringLiteral("bridge"), QVariant());
            if (onClose) {
                onClose();
            } else {
                QTimer::singleShot(0, qmlList, [qmlList]() {
                    qmlList->setSource(QUrl(QStringLiteral("qrc:/AdressListView.qml")));
                });
            }
            bridge->deleteLater();  // Nach onClose – QML-Kontext ist bereits gewechselt
        });

    QTimer::singleShot(0, qmlList, [qmlList]() {
        qmlList->setSource(QUrl(QStringLiteral("qrc:/AdressFormDialog.qml")));
    });
}

void AdressTable::openAndroidQmlDialog(Adresse *adresse,
                                       std::function<void(int)> onSaved)
{
    bool isNew = (adresse == nullptr);

    auto snackFn = [this](const QString &text, int ms) {
        Snackbar::show(this, text, ms);
    };
    auto onSavedFn = [this, onSaved](int newId) {
        refresh();
        emit dataChanged();
        emit savedToDb();
        if (onSaved) onSaved(newId);
    };
    auto onDeletedFn = [this]() {
        refresh();
        emit dataChanged();
        emit savedToDb();
        Snackbar::show(this, tr("Adresse geloescht"), 2000);
    };

    runAdressQmlView(adresse, isNew, m_db, this,
                     std::move(onSavedFn),
                     std::move(onDeletedFn),
                     std::move(snackFn),
                     m_qmlList);
}

void AdressTable::openNewAdressDialog(std::function<void(int)> onSaved)
{
    openAndroidQmlDialog(nullptr, std::move(onSaved));
}

/*static*/ void AdressTable::showNewAdressDialog(
        Database *db, QWidget *parentWidget,
        std::function<void(int)> onSaved)
{
    auto snackFn = [parentWidget](const QString &text, int ms) {
        Snackbar::show(parentWidget, text, ms);
    };
    runAdressQmlView(nullptr, /*isNew=*/true, db, parentWidget,
                     std::move(onSaved),
                     /*onDeleted=*/{},
                     std::move(snackFn));
}

#endif // Q_OS_ANDROID

// ─── Slots ───────────────────────────────────────────────────────────────────

void AdressTable::onAdd()  { openDialog(nullptr); }

void AdressTable::onEdit()
{
#if defined(Q_OS_ANDROID)
    // Android: Edit wird via listBridge::editRequested(id) ausgelöst.
    Q_UNUSED(this)
#else
    int row = m_table->currentRow();
    if (row < 0) {
        QMessageBox::information(this, tr("Hinweis"), tr("Bitte eine Zeile auswählen."));
        return;
    }
    auto *item = m_table->item(row, C_BEZ);
    if (!item) return;
    int id = item->data(Qt::UserRole).toInt();
    Adresse a = m_db->getAdresseById(id);
    openDialog(&a);
#endif
}

void AdressTable::onDelete()
{
#if defined(Q_OS_ANDROID)
    // Android: Löschen erfolgt im AdressFormDialog.
    Q_UNUSED(this)
#else
    int row = m_table->currentRow();
    if (row < 0) {
        QMessageBox::information(this, tr("Hinweis"), tr("Bitte eine Zeile auswählen."));
        return;
    }
    auto *item = m_table->item(row, C_BEZ);
    if (!item) return;
    int id      = item->data(Qt::UserRole).toInt();
    QString bez = item->text().isEmpty()
                  ? (m_table->item(row, C_ORT) ? m_table->item(row, C_ORT)->text() : QString())
                  : item->text();

    if (m_db->adresseInFahrtenUsed(id)) {
        QMessageBox::warning(this, tr("Löschen nicht möglich"),
            tr("Adresse \"%1\" wird in Fahrten verwendet.\n"
               "Bitte erst die Fahrten löschen oder andere Adressen zuweisen.").arg(bez));
        return;
    }
    if (QMessageBox::question(this, tr("Löschen"),
            tr("Adresse \"%1\" wirklich löschen?").arg(bez),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) return;
    if (id > 0 && !m_db->deleteAdresse(id)) {
        QMessageBox::warning(this, tr("Fehler"), tr("Adresse konnte nicht gelöscht werden."));
        return;
    }
    m_table->removeRow(row);
    emit dataChanged();
    emit savedToDb();
#endif
}

void AdressTable::onImportCsv()
{
    QString path = QFileDialog::getOpenFileName(this, tr("CSV-Datei wählen"), QString(),
                   tr("CSV-Dateien (*.csv);;Alle Dateien (*)"));
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Fehler"), tr("Datei konnte nicht geöffnet werden."));
        return;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QTextStream in(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
    in.setEncoding(QStringConverter::Utf8);
#else
    in.setCodec("UTF-8");
#endif
    if (!in.atEnd()) in.readLine();

    QSet<QString> existingKeys;
    for (const Adresse &ex : m_db->getAllAdressen())
        existingKeys.insert(ex.bezeichnung + ex.plz + ex.ort + ex.strasse);

    int imported = 0, skipped = 0;
    while (!in.atEnd()) {
        QString line = in.readLine();
        if (line.trimmed().isEmpty()) { skipped++; continue; }

        QStringList fields;
        QString cur; bool inQ = false;
        for (int i = 0; i < line.length(); ++i) {
            QChar c = line[i];
            if (c == QLatin1Char('"')) {
                if (inQ && i+1 < line.length() && line[i+1] == QLatin1Char('"'))
                    { cur += QLatin1Char('"'); i++; }
                else inQ = !inQ;
            } else if ((c == QLatin1Char(';') || c == QLatin1Char(',')) && !inQ) {
                fields.append(cur.trimmed()); cur.clear();
            } else cur += c;
        }
        fields.append(cur.trimmed());

        if (fields.size() < 5) { skipped++; continue; }

        Adresse a;
        a.bezeichnung = fields.value(0); a.strasse = fields.value(1);
        a.hausnummer  = fields.value(2); a.plz     = fields.value(3);
        a.ort         = fields.value(4); a.land    = fields.value(5, QStringLiteral("Deutschland"));
        if (a.land.isEmpty()) a.land = QStringLiteral("Deutschland");
        if (a.ort.isEmpty())  { skipped++; continue; }
        if (existingKeys.contains(a.bezeichnung + a.plz + a.ort + a.strasse))
            { skipped++; continue; }
        if (m_db->insertAdresse(a)) imported++; else skipped++;
    }
    file.close();
    QApplication::restoreOverrideCursor();

    QMessageBox::information(this, tr("CSV-Import"),
        tr("Import abgeschlossen.\nImportiert: %1\nÜbersprungen: %2")
        .arg(imported).arg(skipped));

    if (imported > 0) {
        refresh();
        emit dataChanged();
        emit savedToDb();
    }
}

// MOC für alle Q_OBJECT-Klassen in diesem .cpp
#if defined(Q_OS_ANDROID)
#include "adresstable.moc"
#endif
