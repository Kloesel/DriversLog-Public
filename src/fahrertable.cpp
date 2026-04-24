#include "fahrertable.h"
#include "touchscroll.h"
#include "snackbar.h"
#include <QMenu>
#include <QPointer>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <algorithm>
#include <memory>
#if defined(Q_OS_ANDROID)
#  include <QQuickView>
#  include <QQuickItem>
#  include <QQmlContext>
#endif

// ─── Android: FahrerListBridge ───────────────────────────────────────────────
#if defined(Q_OS_ANDROID)
class FahrerListBridge : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList fahrer    READ fahrer    NOTIFY fahrerChanged)
    Q_PROPERTY(QString  sortField     READ sortField  NOTIFY sortChanged)
    Q_PROPERTY(bool     sortAsc       READ sortAsc    NOTIFY sortChanged)

public:
    explicit FahrerListBridge(QObject *parent = nullptr) : QObject(parent) {}

    QVariantList fahrer()    const { return m_fahrer;    }
    QString      sortField() const { return m_sortField; }
    bool         sortAsc()   const { return m_sortAsc;   }

    void setFahrer(const QVector<Fahrer> &list) {
        QVector<Fahrer> sorted = list;
        std::sort(sorted.begin(), sorted.end(), [this](const Fahrer &a, const Fahrer &b) {
            return m_sortAsc ? (a.name.toLower() < b.name.toLower())
                             : (a.name.toLower() > b.name.toLower());
        });

        m_fahrer.clear();
        for (const Fahrer &f : sorted) {
            QVariantMap m;
            m[QStringLiteral("id")]   = f.id;
            m[QStringLiteral("name")] = f.isDefault ? tr("unbekannt") : f.name;
            m_fahrer.append(m);
        }
        emit fahrerChanged();
    }

signals:
    void fahrerChanged();
    void sortChanged();
    void addRequested();
    void editRequested(int id);

public slots:
    Q_INVOKABLE void requestAdd()        { emit addRequested(); }
    Q_INVOKABLE void requestEdit(int id) { emit editRequested(id); }
    Q_INVOKABLE void setSort(const QString &field, bool asc) {
        if (m_sortField == field && m_sortAsc == asc) return;
        m_sortField = field;
        m_sortAsc   = asc;
        emit sortChanged();
    }

private:
    QVariantList m_fahrer;
    QString      m_sortField = QStringLiteral("name");
    bool         m_sortAsc   = true;
};

// ─── Android: FahrerFormBridge ───────────────────────────────────────────────
class FahrerFormBridge : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool    isNew      MEMBER m_isNew      CONSTANT)
    Q_PROPERTY(bool    isDefault  READ   isDefault    CONSTANT)
    Q_PROPERTY(QString initName   MEMBER m_initName   CONSTANT)

public:
    explicit FahrerFormBridge(
            const Fahrer            &f,
            bool                     isNew,
            Database                *db,
            std::function<void()>    onSaved,
            std::function<void()>    onDeleted,
            QObject                 *parent = nullptr)
        : QObject(parent)
        , m_fahrerId(f.id)
        , m_isNew(isNew)
        , m_initName(f.isDefault ? tr("unbekannt") : f.name)
        , m_db(db)
        , m_onSaved(std::move(onSaved))
        , m_onDeleted(std::move(onDeleted))
    {}

signals:
    void closeRequested();
    void snackbarRequested(QString text, int ms);
    void savedWithId(int id);   // nach Speichern: neue/aktualisierte Fahrer-ID

public slots:
    Q_INVOKABLE void save(const QString &name)
    {
        Fahrer f;
        f.id   = m_fahrerId;
        f.name = name.trimmed();

        bool ok = m_isNew ? m_db->insertFahrer(f) : m_db->updateFahrer(f);
        if (!ok) {
            emit snackbarRequested(tr("Fahrer konnte nicht gespeichert werden."), 3000);
            return;
        }
        if (m_onSaved) m_onSaved();
        emit savedWithId(f.id);
        emit snackbarRequested(
            m_isNew ? tr("Fahrer gespeichert") : tr("Fahrer aktualisiert"), 2500);
        emit closeRequested();
    }

    Q_INVOKABLE void deleteFahrer()
    {
        if (m_fahrerId > 0 && !m_db->deleteFahrer(m_fahrerId)) {
            emit snackbarRequested(tr("Fahrer konnte nicht geloescht werden."), 3000);
            return;
        }
        if (m_onDeleted) m_onDeleted();
        emit snackbarRequested(tr("Fahrer geloescht"), 2000);
        emit closeRequested();
    }

    Q_INVOKABLE void cancel() { emit closeRequested(); }

    bool isDefault() const { return m_fahrerId == m_db->getDefaultFahrerId(); }

private:
    int      m_fahrerId = 0;
    bool     m_isNew;
    QString  m_initName;
    Database *m_db;
    std::function<void()> m_onSaved;
    std::function<void()> m_onDeleted;
};
#endif // Q_OS_ANDROID

// ─── Konstruktor ─────────────────────────────────────────────────────────────

FahrerTable::FahrerTable(Database *db, SyncManager *syncMgr, QWidget *parent)
    : QWidget(parent), m_db(db), m_syncMgr(syncMgr)
{
    auto *layout = new QVBoxLayout(this);

#if defined(Q_OS_ANDROID)
    // ── Android: QML-ListView ─────────────────────────────────────────────────
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_listBridge = new FahrerListBridge(this);

    connect(m_listBridge, &FahrerListBridge::addRequested,
            this, &FahrerTable::onAdd);
    connect(m_listBridge, &FahrerListBridge::editRequested,
            this, [this](int id) {
                Fahrer f = m_db->getFahrerById(id);
                openDialog(&f);
            });
    connect(m_listBridge, &FahrerListBridge::sortChanged,
            this, &FahrerTable::refresh);

    m_qmlList = new QQuickWidget(this);
    m_qmlList->setAttribute(Qt::WA_AcceptTouchEvents);
    m_qmlList->setResizeMode(QQuickWidget::SizeRootObjectToView);
    m_qmlList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_qmlList->rootContext()->setContextProperty(
        QStringLiteral("listBridge"), m_listBridge);
    m_qmlList->setSource(QUrl(QStringLiteral("qrc:/FahrerListView.qml")));

    connect(m_qmlList, &QQuickWidget::statusChanged, this,
            [this](QQuickWidget::Status s) {
                if (s == QQuickWidget::Error)
                    for (const auto &e : m_qmlList->errors())
                        qWarning() << "FahrerListView QML:" << e.toString();
            });

    layout->addWidget(m_qmlList);

    // Dummy-Buttons (verhindert nullptr-Crashes)
    m_addBtn  = new QPushButton(this); m_addBtn->hide();
    m_editBtn = new QPushButton(this); m_editBtn->hide();
    m_delBtn  = new QPushButton(this); m_delBtn->hide();

#else
    // ── Desktop: QTableWidget ─────────────────────────────────────────────────
    layout->setContentsMargins(4, 4, 4, 4);

    auto *toolbar = new QHBoxLayout();
    m_addBtn  = new QPushButton(tr("+ Neuer Fahrer"));
    m_editBtn = new QPushButton(tr("✎ Bearbeiten"));
    m_delBtn  = new QPushButton(tr("Löschen"));
    toolbar->addWidget(m_addBtn);
    toolbar->addWidget(m_editBtn);
    toolbar->addWidget(m_delBtn);
    toolbar->addStretch();
    layout->addLayout(toolbar);

    m_table = new QTableWidget(0, C_COUNT, this);
    enableTouchScroll(m_table);
    m_table->setHorizontalHeaderLabels({tr("Name")});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSortingEnabled(true);
    m_table->setColumnWidth(C_NAME, 220);
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

    connect(m_addBtn,  &QPushButton::clicked, this, &FahrerTable::onAdd);
    connect(m_editBtn, &QPushButton::clicked, this, &FahrerTable::onEdit);
    connect(m_delBtn,  &QPushButton::clicked, this, &FahrerTable::onDelete);

    // Doppelklick-Fix: Index korrekt auswerten
    connect(m_table, &QTableWidget::doubleClicked, this,
            [this](const QModelIndex &idx) {
                if (!idx.isValid()) return;
                m_table->selectRow(idx.row());
                onEdit();
            });
#endif
}

// ─── Hilfsmethoden ───────────────────────────────────────────────────────────

void FahrerTable::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
}

void FahrerTable::refresh()
{
    populateTable(m_db->getAllFahrer());
}

void FahrerTable::populateTable(const QVector<Fahrer> &list)
{
#if defined(Q_OS_ANDROID)
    m_listBridge->setFahrer(list);
#else
    m_table->setSortingEnabled(false);
    m_table->setRowCount(0);
    for (const Fahrer &f : list) {
        int row = m_table->rowCount();
        m_table->insertRow(row);
        auto *n = new QTableWidgetItem(f.isDefault ? tr("unbekannt") : f.name);
        n->setData(Qt::UserRole, f.id);
        m_table->setItem(row, C_NAME, n);
    }
    m_table->setSortingEnabled(true);
#endif
}

// ─── openDialog ──────────────────────────────────────────────────────────────

void FahrerTable::openDialog(Fahrer *fahrer)
{
#if defined(Q_OS_ANDROID)
    openAndroidQmlDialog(fahrer);
#else
    bool isNew = (fahrer == nullptr);
    Fahrer f;
    if (!isNew) f = *fahrer;

    auto *dlg = new QDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle(isNew ? tr("Neuer Fahrer") : tr("Fahrer bearbeiten"));
    dlg->setMinimumWidth(320);

    auto *lay = new QFormLayout(dlg);
    lay->setSpacing(10);
    lay->setContentsMargins(16, 16, 16, 16);

    auto *nameEdit = new QLineEdit(f.name);
    nameEdit->setPlaceholderText(tr("Pflichtfeld"));

    lay->addRow(tr("Name*:"), nameEdit);

    auto *bb = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    bb->button(QDialogButtonBox::Save)->setText(tr("Speichern"));
    bb->button(QDialogButtonBox::Save)->setEnabled(!f.name.isEmpty());
    connect(nameEdit, &QLineEdit::textChanged, bb->button(QDialogButtonBox::Save),
        [bb](const QString &t) {
            bb->button(QDialogButtonBox::Save)->setEnabled(!t.trimmed().isEmpty());
        });
    lay->addRow(bb);

    auto savedToDB = std::make_shared<bool>(false);

    QObject::connect(bb->button(QDialogButtonBox::Cancel), &QPushButton::clicked, dlg,
        [this, dlg, savedToDB]() {
            if (*savedToDB) { emit dataChanged(); emit savedToDb(); }
            dlg->reject();
        });

    QObject::connect(bb->button(QDialogButtonBox::Save), &QPushButton::clicked, dlg,
        [this, dlg, isNew, f, nameEdit, savedToDB]() mutable {
            if (!*savedToDB) {
                f.name = nameEdit->text().trimmed();
                bool ok = isNew ? m_db->insertFahrer(f) : m_db->updateFahrer(f);
                if (!ok) {
                    QMessageBox::warning(this, tr("Fehler"),
                        tr("Fahrer konnte nicht gespeichert werden."));
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

// ─── Android: QML-Dialog ─────────────────────────────────────────────────────
#if defined(Q_OS_ANDROID)
void FahrerTable::openAndroidQmlDialog(Fahrer *fahrer)
{
    // Qt 6.8 / Android 16: Source-Swap statt QQuickView (kein EGL-Konflikt)
    if (!m_qmlList) return;

    bool   isNew = (fahrer == nullptr);
    Fahrer f;
    if (!isNew) f = *fahrer;

    auto *bridge = new FahrerFormBridge(
        f, isNew, m_db,
        [this]() { refresh(); emit dataChanged(); emit savedToDb(); },
        [this]() { refresh(); emit dataChanged(); emit savedToDb(); },
        m_qmlList);

    m_qmlList->rootContext()->setContextProperty(QStringLiteral("bridge"), bridge);

    QObject::connect(bridge, &FahrerFormBridge::snackbarRequested,
        this, [this](const QString &text, int ms) {
            Snackbar::show(this, text, ms);
        });

    QObject::connect(bridge, &FahrerFormBridge::closeRequested,
        this, [this, bridge]() {
            bridge->deleteLater();
            m_qmlList->rootContext()->setContextProperty("bridge", QVariant());
            m_qmlList->rootContext()->setContextProperty(QStringLiteral("listBridge"), m_listBridge);
            QTimer::singleShot(0, this, [this]() {
                m_qmlList->setSource(QUrl(QStringLiteral("qrc:/FahrerListView.qml")));
                refresh();
            });
        });

    QTimer::singleShot(0, this, [this]() {
        m_qmlList->setSource(QUrl(QStringLiteral("qrc:/FahrerFormDialog.qml")));
    });
}

/*static*/ void FahrerTable::runFahrerQmlView(
        Database                                *db,
        QQuickWidget                            *qmlList,
        std::function<void(int)>                 onSaved,
        std::function<void(const QString&,int)>  showSnackbar,
        std::function<void()>                    onClose)
{
    if (!qmlList) return;

    Fahrer f;
    auto *bridge = new FahrerFormBridge(f, /*isNew=*/true, db, {}, {}, qmlList);

    QObject::connect(bridge, &FahrerFormBridge::snackbarRequested,
        qmlList, [showSnackbar](const QString &text, int ms) {
            if (showSnackbar) showSnackbar(text, ms);
        });

    QObject::connect(bridge, &FahrerFormBridge::savedWithId,
        qmlList, [onSaved](int id) {
            if (onSaved) onSaved(id);
        });

    QObject::connect(bridge, &FahrerFormBridge::closeRequested,
        qmlList, [qmlList, bridge, onClose]() {
            qmlList->rootContext()->setContextProperty(QStringLiteral("bridge"), QVariant());
            bridge->deleteLater();
            if (onClose) onClose();
        });

    qmlList->rootContext()->setContextProperty(QStringLiteral("bridge"), bridge);
    QTimer::singleShot(0, qmlList, [qmlList]() {
        qmlList->setSource(QUrl(QStringLiteral("qrc:/FahrerFormDialog.qml")));
    });
}
#endif

// ─── Slots ───────────────────────────────────────────────────────────────────

void FahrerTable::onAdd() { openDialog(nullptr); }

void FahrerTable::onEdit()
{
#if defined(Q_OS_ANDROID)
    Q_UNUSED(this)
#else
    int row = m_table->currentRow();
    if (row < 0) {
        QMessageBox::information(this, tr("Hinweis"), tr("Bitte eine Zeile auswählen."));
        return;
    }
    auto *item = m_table->item(row, C_NAME);
    if (!item) return;
    Fahrer f = m_db->getFahrerById(item->data(Qt::UserRole).toInt());
    openDialog(&f);
#endif
}

void FahrerTable::onDelete()
{
#if defined(Q_OS_ANDROID)
    Q_UNUSED(this)
#else
    int row = m_table->currentRow();
    if (row < 0) {
        QMessageBox::information(this, tr("Hinweis"), tr("Bitte eine Zeile auswählen."));
        return;
    }
    auto *item = m_table->item(row, C_NAME);
    if (!item) return;
    int id = item->data(Qt::UserRole).toInt();
    QString name = item->text();


    if (id == m_db->getDefaultFahrerId()) {
        QMessageBox::information(this, tr("Hinweis"),
            tr("Der Standard-Fahrer kann nicht gelöscht werden."));
        return;
    }

    if (QMessageBox::question(this, tr("Löschen"),
            tr("Fahrer \"%1\" wirklich löschen?").arg(name),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) return;

    if (id > 0 && !m_db->deleteFahrer(id)) {
        QMessageBox::warning(this, tr("Fehler"), tr("Fahrer konnte nicht gelöscht werden."));
        return;
    }
    m_table->removeRow(row);
    emit dataChanged();
    emit savedToDb();
#endif
}

// MOC für alle Q_OBJECT-Klassen in diesem .cpp
#if defined(Q_OS_ANDROID)
#include "fahrertable.moc"
#endif
