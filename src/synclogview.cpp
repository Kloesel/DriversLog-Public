#include "synclogview.h"
#include <QTimer>
#include <QPointer>
#include <QShowEvent>
#include <QHideEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QApplication>
#include <QSettings>
#include <QScreen>
#include <QSet>
#include <QSqlQuery>
#include <QTabWidget>

// Sortiert QTableWidgetItem nach Qt::UserRole (numerisch/chronologisch)
// statt alphabetisch nach Anzeigetext.
class NumericSortItem : public QTableWidgetItem
{
public:
    explicit NumericSortItem(const QString &text, qint64 sortKey)
        : QTableWidgetItem(text)
    {
        setData(Qt::UserRole, sortKey);
    }
    bool operator<(const QTableWidgetItem &other) const override
    {
        return data(Qt::UserRole).toLongLong() <
               other.data(Qt::UserRole).toLongLong();
    }
};
#if defined(Q_OS_ANDROID)
#  include <QQuickWidget>
#  include <QQmlContext>
#  include <QQuickItem>
#  include "snackbar.h"

// ─── SyncLogBridge ────────────────────────────────────────────────────────────
class SyncLogBridge : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList entries        READ entries        NOTIFY entriesChanged)
    Q_PROPERTY(QVariantList knowledgeRows  READ knowledgeRows  NOTIFY knowledgeChanged)
    Q_PROPERTY(QVariantList filterModel    READ filterModel    CONSTANT)
    Q_PROPERTY(QString      summary        READ summary        NOTIFY summaryChanged)
    Q_PROPERTY(QString      matrixSummary  READ matrixSummary  NOTIFY knowledgeChanged)

public:
    explicit SyncLogBridge(DatabaseSync *dbSync, QObject *parent = nullptr)
        : QObject(parent), m_dbSync(dbSync)
    {
        // Filter-Modell: 0=Alle 1=Lokal 2=Empfangen 3=Ausstehend 4=Matrix
        auto mk = [](int id, const QString &txt) {
            QVariantMap m;
            m[QStringLiteral("id")]   = id;
            m[QStringLiteral("text")] = txt;
            return m;
        };
        m_filterModel << mk(0, tr("Alle"))
                      << mk(1, tr("Lokal"))
                      << mk(2, tr("Empf."))
                      << mk(3, tr("Ausst."))
                      << mk(4, tr("Matrix"));
    }

    QVariantList entries()       const { return m_entries;       }
    QVariantList knowledgeRows() const { return m_knowledgeRows; }
    QVariantList filterModel()   const { return m_filterModel;   }
    QString      summary()       const { return m_summary;       }
    QString      matrixSummary() const { return m_matrixSummary; }

    void load(int filterIdx = 0)
    {
        if (!m_dbSync) return;

        if (filterIdx == 4) {
            loadMatrix();
            return;
        }

        const QList<SyncLogEntry> all = m_dbSync->getSyncLog(500);

        // Zusammenfassung
        int loc = 0, rem = 0, pend = 0;
        for (const SyncLogEntry &e : all) {
            if (e.direction == QLatin1String("local"))   loc++;
            if (e.direction == QLatin1String("remote"))  rem++;
            if (e.status    == QLatin1String("pending")) pend++;
        }
        m_summary = tr("Gesamt: %1  |  Lokal: %2  |  Empf.: %3  |  Ausst.: %4  |  Seq: %5")
                    .arg(all.size()).arg(loc).arg(rem).arg(pend)
                    .arg(m_dbSync->maxLocalSeq());
        const int migCount = m_dbSync->migrationEntryCount();
        if (migCount > 0)
            m_summary += tr("  (+%1 Migr.)").arg(migCount);
        emit summaryChanged();

        m_entries.clear();
        for (const SyncLogEntry &e : all) {
            bool keep = false;
            switch (filterIdx) {
                case 0: keep = true; break;
                case 1: keep = (e.direction == QLatin1String("local"));   break;
                case 2: keep = (e.direction == QLatin1String("remote"));  break;
                case 3: keep = (e.status    == QLatin1String("pending")); break;
            }
            if (!keep) continue;

            QVariantMap m;
            m[QStringLiteral("ts")]          = e.timestamp.toString(QStringLiteral("dd.MM. HH:mm"));
            m[QStringLiteral("table")]       = tableText(e.tableName);
            m[QStringLiteral("op")]          = opText(e.operation);
            m[QStringLiteral("opColor")]     = opColor(e.operation);
            m[QStringLiteral("dir")]         = (e.direction == QLatin1String("local"))
                                               ? tr("lokal") : tr("empf.");
            m[QStringLiteral("dirColor")]    = (e.direction == QLatin1String("local"))
                                               ? QStringLiteral("#1a7fc1")
                                               : QStringLiteral("#27ae60");
            m[QStringLiteral("status")]      = statusText(e.status);
            m[QStringLiteral("statusColor")] = statusColor(e.status);
            m[QStringLiteral("device")]      = e.deviceId.left(8);
            m[QStringLiteral("seq")]         = e.localSeq > 0
                                               ? QString::number(e.localSeq)
                                               : QStringLiteral("–");
            m[QStringLiteral("device")]      = e.deviceName;
            m_entries.append(m);
        }
        emit entriesChanged();
    }

private:
    void loadMatrix()
    {
        const QList<KnowledgeEntry> matrix = m_dbSync->getKnowledgeMatrix();

        int totalPending = 0;
        QSet<QString> peerSet;
        for (const KnowledgeEntry &e : matrix) {
            if (e.peerDeviceId != m_dbSync->deviceId())
                peerSet.insert(e.peerDeviceId);
            totalPending += e.pendingCount;
        }

        const QString syncStatus = (totalPending == 0)
            ? QStringLiteral("✓ Alle vollst.")
            : tr("⚠ %1 ausst.").arg(totalPending);
        m_matrixSummary = tr("Peers: %1  |  Seq: %2  |  %3")
                          .arg(peerSet.size()).arg(m_dbSync->maxLocalSeq())
                          .arg(syncStatus);
        emit knowledgeChanged();

        m_knowledgeRows.clear();
        for (const KnowledgeEntry &e : matrix) {
            const QString seqText = QString("%1 / %2").arg(e.originMaxSeq).arg(e.maxSeq);

            QString statusText, statusColor;
            if (e.isSelf) {
                statusText  = QObject::tr("lokal");
                statusColor = QStringLiteral("#1F4E79");
            } else if (e.originMaxSeq == 0) {
                statusText  = QStringLiteral("–");
                statusColor = QStringLiteral("#aaa");
            } else if (e.pendingCount == 0) {
                statusText  = QStringLiteral("\u2713");  // ✓
                statusColor = QStringLiteral("#27ae60");
            } else {
                statusText  = QObject::tr("%1 fehlen").arg(e.pendingCount);
                statusColor = QStringLiteral("#e67e22");
            }

            QVariantMap m;
            m[QStringLiteral("peer")]        = e.peerName;
            m[QStringLiteral("origin")]      = e.originName;
            m[QStringLiteral("seq")]         = e.maxSeq;
            m[QStringLiteral("seqText")]     = seqText;
            m[QStringLiteral("statusText")]  = statusText;
            m[QStringLiteral("statusColor")] = statusColor;
            m[QStringLiteral("isSelf")]      = e.isSelf;
            m[QStringLiteral("seqColor")]    = e.isSelf
                ? QStringLiteral("#1F4E79")
                : (e.pendingCount == 0 && e.originMaxSeq > 0)
                  ? QStringLiteral("#27ae60")
                  : e.pendingCount > 0 ? QStringLiteral("#e67e22")
                                       : QStringLiteral("#aaa");
            m[QStringLiteral("peerFull")]    = e.peerDeviceId;
            m[QStringLiteral("originFull")]  = e.originDeviceId;
            m_knowledgeRows.append(m);
        }
        emit knowledgeChanged();
    }

signals:
    void entriesChanged();
    void summaryChanged();
    void knowledgeChanged();

public slots:
    Q_INVOKABLE void applyFilter(int idx) { m_currentFilter = idx; load(idx); }
    // Wird vom SyncLogView-Timer + changeApplied-Signal aufgerufen
    Q_INVOKABLE void reload() { load(m_currentFilter); }

    Q_INVOKABLE void pruneAndReload()
    {
        if (m_dbSync) m_dbSync->pruneLog(30);
        load(m_currentFilter);
    }

private:
    static QString tableText(const QString &t) {
        if (t == QLatin1String("fahrten"))  return QObject::tr("Fahrten");
        if (t == QLatin1String("adressen")) return QObject::tr("Adressen");
        if (t == QLatin1String("fahrer"))   return QObject::tr("Fahrer");
        return t;
    }
    static QString opText(const QString &op) {
        if (op == QLatin1String("INSERT")) return QObject::tr("Neu");
        if (op == QLatin1String("UPDATE")) return QObject::tr("Geänd.");
        if (op == QLatin1String("DELETE")) return QObject::tr("Gelöscht");
        return op;
    }
    static QString opColor(const QString &op) {
        if (op == QLatin1String("INSERT")) return QStringLiteral("#27ae60");
        if (op == QLatin1String("UPDATE")) return QStringLiteral("#1a7fc1");
        if (op == QLatin1String("DELETE")) return QStringLiteral("#c0392b");
        return QStringLiteral("#1A1C1E");
    }
    static QString statusText(const QString &s) {
        if (s == QLatin1String("pending"))  return QObject::tr("ausst.");
        if (s == QLatin1String("sent"))     return QObject::tr("gesendet");
        if (s == QLatin1String("received")) return QObject::tr("empf.");
        return s;
    }
    static QString statusColor(const QString &s) {
        if (s == QLatin1String("pending"))  return QStringLiteral("#e67e22");
        if (s == QLatin1String("sent"))     return QStringLiteral("#1a7fc1");
        if (s == QLatin1String("received")) return QStringLiteral("#27ae60");
        return QStringLiteral("#1A1C1E");
    }

    DatabaseSync  *m_dbSync;
    QVariantList   m_entries;
    QVariantList   m_knowledgeRows;
    QVariantList   m_filterModel;
    QString        m_summary;
    QString        m_matrixSummary;
    int            m_currentFilter = 0; // zuletzt gewählter Filter – für Reload merken
};
#endif // Q_OS_ANDROID

// ─── SyncLogView ──────────────────────────────────────────────────────────────

SyncLogView::SyncLogView(DatabaseSync *dbSync, QWidget *parent)
    : QDialog(parent)
    , m_dbSync(dbSync)
{
    setWindowTitle(tr("Sync-Protokoll"));

#if defined(Q_OS_ANDROID)
    m_bridge = new SyncLogBridge(dbSync, this);
    m_bridge->load();

    // Alle 3 s aktualisieren + sofort bei eingehender Änderung
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(3000);
    connect(m_refreshTimer, &QTimer::timeout, m_bridge, &SyncLogBridge::reload);
    connect(dbSync, &DatabaseSync::changeApplied, this, [this](const QString &, int) {
        if (m_bridge) m_bridge->reload();
    });

    // Qt 6.8 / Android 16: QQuickWidget statt QQuickView.
    // QQuickWidget teilt den EGL-Context aller anderen QQuickWidgets → kein Konflikt,
    // kein Timer-Hack, kein releaseResources() nötig.
    auto *pw = qobject_cast<QWidget*>(parent);
    auto *qw = new QQuickWidget(pw);
    qw->setAttribute(Qt::WA_DeleteOnClose);
    qw->setResizeMode(QQuickWidget::SizeRootObjectToView);
    qw->rootContext()->setContextProperty(QStringLiteral("logBridge"), m_bridge);

    connect(qw, &QQuickWidget::statusChanged, qw,
        [qw](QQuickWidget::Status s) {
            if (s == QQuickWidget::Error)
                for (const auto &e : qw->errors())
                    qWarning() << "SyncLogView QML:" << e.toString();
            if (s != QQuickWidget::Ready) return;
            if (auto *root = qw->rootObject())
                QObject::connect(root, SIGNAL(closeRequested()),
                                 qw, SLOT(deleteLater()));
        });

    connect(qw, &QObject::destroyed, this,
        [this]() { if (m_refreshTimer) m_refreshTimer->stop(); });

    qw->setSource(QUrl(QStringLiteral("qrc:/SyncLogView.qml")));
    if (pw) qw->setGeometry(0, 0, pw->width(), pw->height());
    qw->show();
    qw->raise();
    hide();

    // Auf Android verbirgt sich SyncLogView sofort selbst (hide() oben),
    // was hideEvent → m_refreshTimer->stop() auslöst.
    // Timer hier explizit (neu) starten – er läuft bis QQuickWidget destroyed.
    m_refreshTimer->start();

#else
    setModal(false);
    setupUi();
    refresh();

    // Alle 3 s aktualisieren + sofort bei eingehender Änderung
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(3000);
    connect(m_refreshTimer, &QTimer::timeout, this, &SyncLogView::refresh);
    connect(dbSync, &DatabaseSync::changeApplied, this, [this](const QString &, int) {
        refresh();
    });
#if defined(Q_OS_WIN)
    {
        QSettings s;
        const QByteArray geo = s.value(QStringLiteral("synclog/geometry")).toByteArray();
        if (!geo.isEmpty()) {
            restoreGeometry(geo);
        } else {
            const QSize screen = QApplication::primaryScreen()->availableSize();
            resize(qMax(800, screen.width()  * 9 / 10),
                   qMax(500, screen.height() * 8 / 10));
        }
        const QByteArray logHdr = s.value(QStringLiteral("synclog/log_header")).toByteArray();
        if (!logHdr.isEmpty())
            m_logTable->horizontalHeader()->restoreState(logHdr);
        const QByteArray matHdr = s.value(QStringLiteral("synclog/matrix_header")).toByteArray();
        if (!matHdr.isEmpty())
            m_matrixTable->horizontalHeader()->restoreState(matHdr);
    }
#else
    {
        const QSize screen = QApplication::primaryScreen()->availableSize();
        resize(qMax(800, screen.width()  * 9 / 10),
               qMax(500, screen.height() * 8 / 10));
    }
#endif
#endif
}

// ─── Refresh ──────────────────────────────────────────────────────────────────

void SyncLogView::refresh()
{
#if defined(Q_OS_ANDROID)
    if (m_bridge) m_bridge->load();
#else
    if (!m_dbSync) return;
    refreshLog();
    refreshMatrix();
#endif
}

#if !defined(Q_OS_ANDROID)

void SyncLogView::refreshLog()
{
    const QString filter = m_filterCb->currentData().toString();
    const QList<SyncLogEntry> all = m_dbSync->getSyncLog(500);

    QList<SyncLogEntry> entries;
    for (const SyncLogEntry &e : all) {
        if      (filter == "all")    entries.append(e);
        else if (filter == "local"   && e.direction == "local")   entries.append(e);
        else if (filter == "remote"  && e.direction == "remote")  entries.append(e);
        else if (filter == "pending" && e.status    == "pending") entries.append(e);
    }

    int local = 0, remote = 0, pending = 0;
    for (const SyncLogEntry &e : all) {
        if (e.direction == "local")   local++;
        if (e.direction == "remote")  remote++;
        if (e.status    == "pending") pending++;
    }
    const int migCount = m_dbSync->migrationEntryCount();
    QString summary = tr("Gesamt: %1  |  Lokal: %2  |  Empfangen: %3  |  Ausstehend: %4  |  Eigene Seq: %5")
        .arg(all.size()).arg(local).arg(remote).arg(pending)
        .arg(m_dbSync->maxLocalSeq());
    if (migCount > 0)
        summary += tr("  |  Migration: %1 (ausgeblendet)").arg(migCount);
    m_summaryLabel->setText(summary);

    m_logTable->setSortingEnabled(false);
    m_logTable->setRowCount(entries.size());

    auto opText = [](const QString &op) -> QString {
        if (op == "INSERT") return QObject::tr("Neu");
        if (op == "UPDATE") return QObject::tr("Geändert");
        if (op == "DELETE") return QObject::tr("Gelöscht");
        return op;
    };
    auto tableText = [](const QString &t) -> QString {
        if (t == "fahrten")  return QObject::tr("Fahrten");
        if (t == "adressen") return QObject::tr("Adressen");
        if (t == "fahrer")   return QObject::tr("Fahrer");
        return t;
    };

    for (int row = 0; row < entries.size(); ++row) {
        const SyncLogEntry &e = entries[row];

        // Zeitpunkt (Col 0) – numerisch sortierbar nach epoch ms
        auto *tsItem = new NumericSortItem(
            e.timestamp.toString("dd.MM.yyyy  hh:mm:ss"),
            e.timestamp.toMSecsSinceEpoch());
        m_logTable->setItem(row, 0, tsItem);

        // Tabelle (Col 1)
        m_logTable->setItem(row, 1, new QTableWidgetItem(tableText(e.tableName)));

        // Aktion (Col 2)
        auto *opItem = new QTableWidgetItem(opText(e.operation));
        if      (e.operation == "INSERT") opItem->setForeground(QColor("#27ae60"));
        else if (e.operation == "UPDATE") opItem->setForeground(QColor("#1a7fc1"));
        else if (e.operation == "DELETE") opItem->setForeground(QColor("#c0392b"));
        m_logTable->setItem(row, 2, opItem);

        // Richtung (Col 3)
        const bool isLocal = (e.direction == "local");
        auto *dirItem = new QTableWidgetItem(isLocal ? tr("lokal") : tr("empf."));
        dirItem->setForeground(isLocal ? QColor("#1a7fc1") : QColor("#27ae60"));
        m_logTable->setItem(row, 3, dirItem);

        // Status (Col 4)
        QString statusTxt; QColor statusClr;
        if      (e.status == "pending")  { statusTxt = tr("ausstehend"); statusClr = QColor("#e67e22"); }
        else if (e.status == "sent")     { statusTxt = tr("gesendet");   statusClr = QColor("#1a7fc1"); }
        else if (e.status == "received") { statusTxt = tr("empfangen");  statusClr = QColor("#27ae60"); }
        else                             { statusTxt = e.status;         statusClr = Qt::black; }
        auto *stItem = new QTableWidgetItem(statusTxt);
        stItem->setForeground(statusClr);
        m_logTable->setItem(row, 4, stItem);

        // Seq# (Col 5) – numerisch sortierbar
        auto *seqItem = new NumericSortItem(
            e.localSeq > 0 ? QString::number(e.localSeq) : QStringLiteral("–"),
            e.localSeq);
        seqItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        if (e.localSeq == 0) seqItem->setForeground(QColor("#aaa"));
        m_logTable->setItem(row, 5, seqItem);

        // Gerät (Col 6) – Anzeigename; bei unbekannter UUID kurze ID anzeigen
        const QString devDisplay = e.deviceName.isEmpty()
            ? tr("unbekannt (%1)").arg(e.deviceId.left(8))
            : e.deviceName;
        auto *devItem = new QTableWidgetItem(devDisplay);
        devItem->setToolTip(e.deviceId);
        if (e.deviceName.isEmpty())
            devItem->setForeground(QColor("#aaa"));
        m_logTable->setItem(row, 6, devItem);
    }
    m_logTable->setSortingEnabled(true);
    m_logTable->sortByColumn(m_logSortCol, m_logSortOrder);
}

void SyncLogView::refreshMatrix()
{
    const QList<KnowledgeEntry> matrix = m_dbSync->getKnowledgeMatrix();

    m_matrixTable->setSortingEnabled(false);
    m_matrixTable->setRowCount(matrix.size());

    int totalPending = 0;
    QSet<QString> peers;

    for (int row = 0; row < matrix.size(); ++row) {
        const KnowledgeEntry &e = matrix[row];

        if (!e.isSelf && e.peerDeviceId != m_dbSync->deviceId())
            peers.insert(e.peerDeviceId);
        totalPending += e.pendingCount;

        // Quelle / Ursprungs-Gerät (Col 0) – erste Spalte
        auto *originItem = new QTableWidgetItem(e.originName);
        originItem->setToolTip(e.originDeviceId);
        if (e.isSelf) {
            originItem->setForeground(QColor("#1F4E79"));
            QFont f = originItem->font(); f.setItalic(true); originItem->setFont(f);
        }
        m_matrixTable->setItem(row, 0, originItem);

        // Ziel / Peer-Gerät (Col 1)
        auto *peerItem = new QTableWidgetItem(e.peerName);
        peerItem->setToolTip(e.peerDeviceId);
        if (e.isSelf) {
            peerItem->setForeground(QColor("#1F4E79"));
            QFont f = peerItem->font(); f.setItalic(true); peerItem->setFont(f);
        }
        m_matrixTable->setItem(row, 1, peerItem);

        // Seq: "42 / 42" zeigt peer-Stand / aktuellen Stand des Ursprungsgeräts
        const QString seqText = QString("%1 / %2")
                                .arg(e.originMaxSeq).arg(e.maxSeq);
        auto *seqItem = new QTableWidgetItem(seqText);
        seqItem->setData(Qt::UserRole, e.maxSeq);
        seqItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        if (e.isSelf)
            seqItem->setForeground(QColor("#1F4E79"));
        else if (e.pendingCount == 0 && e.originMaxSeq > 0)
            seqItem->setForeground(QColor("#27ae60"));   // vollständig → grün
        else if (e.pendingCount > 0)
            seqItem->setForeground(QColor("#e67e22"));   // ausstehend → orange
        m_matrixTable->setItem(row, 2, seqItem);

        // Status (Col 3): ✓ vollständig | "N fehlen"
        QString statusText;
        QColor  statusColor;
        if (e.isSelf) {
            statusText  = tr("lokal");
            statusColor = QColor("#1F4E79");
        } else if (e.originMaxSeq == 0) {
            statusText  = tr("–");
            statusColor = QColor("#aaa");
        } else if (e.pendingCount == 0) {
            statusText  = tr("✓ vollst.");
            statusColor = QColor("#27ae60");
        } else {
            statusText  = tr("%1 fehlen").arg(e.pendingCount);
            statusColor = QColor("#e67e22");
        }
        auto *stItem = new QTableWidgetItem(statusText);
        stItem->setForeground(statusColor);
        stItem->setTextAlignment(Qt::AlignCenter);
        if (e.pendingCount == 0 && !e.isSelf && e.originMaxSeq > 0) {
            QFont f = stItem->font(); f.setBold(true); stItem->setFont(f);
        }
        m_matrixTable->setItem(row, 3, stItem);
    }
    m_matrixTable->setSortingEnabled(true);
    m_matrixTable->sortByColumn(m_matSortCol, m_matSortOrder);

    // Zusammenfassung mit Gesamtstatus
    const QString syncStatus = (totalPending == 0)
        ? tr("✓ Alle Peers vollständig synchronisiert")
        : tr("⚠ %1 Einträge noch ausstehend").arg(totalPending);
    m_matrixSummaryLabel->setText(
        tr("Bekannte Peers: %1  |  Eigene Seq: %2  |  %3")
        .arg(peers.size()).arg(m_dbSync->maxLocalSeq()).arg(syncStatus));
}

void SyncLogView::onFilterChanged() { refreshLog(); }

// ── Hilfsklasse: sortiert QTableWidgetItem nach Qt::UserRole (numerisch) ──
// Wird für Datum (epoch ms) und Seq# verwendet damit die Sortierung
// chronologisch/numerisch statt alphabetisch erfolgt.
void SyncLogView::setupUi()
{
    auto *vbox = new QVBoxLayout(this);
    vbox->setSpacing(0);
    vbox->setContentsMargins(4, 4, 4, 4);

    // Titelleiste
    auto *toolbar = new QWidget(this);
    toolbar->setStyleSheet("background: #1F4E79; padding: 4px;");
    auto *tbLay = new QHBoxLayout(toolbar);
    tbLay->setContentsMargins(8, 4, 8, 4);
    auto *titleLbl = new QLabel(tr("Sync-Protokoll"), toolbar);
    titleLbl->setStyleSheet("color: white; font-size: 14pt; font-weight: bold;");
    tbLay->addWidget(titleLbl, 1);
    vbox->addWidget(toolbar);

    // Tab-Widget
    m_tabs = new QTabWidget(this);
    m_tabs->setDocumentMode(true);
    vbox->addWidget(m_tabs, 1);

    // ── Tab 1: Änderungslog ────────────────────────────────────────────────
    auto *logWidget = new QWidget();
    auto *logVBox   = new QVBoxLayout(logWidget);
    logVBox->setContentsMargins(4, 4, 4, 4);
    logVBox->setSpacing(4);

    // Filter-Leiste
    auto *filterBar = new QWidget();
    auto *fbLay     = new QHBoxLayout(filterBar);
    fbLay->setContentsMargins(0, 0, 0, 0);
    fbLay->addWidget(new QLabel(tr("Filter:")));
    m_filterCb = new QComboBox();
    m_filterCb->addItem(tr("Alle"),        "all");
    m_filterCb->addItem(tr("Lokal"),       "local");
    m_filterCb->addItem(tr("Empfangen"),   "remote");
    m_filterCb->addItem(tr("Ausstehend"),  "pending");
    connect(m_filterCb, &QComboBox::currentIndexChanged,
            this, &SyncLogView::onFilterChanged);
    m_refreshBtn = new QPushButton(tr("Aktualisieren"));
    m_refreshBtn->setMinimumWidth(110);
    m_refreshBtn->setMinimumHeight(32);
    connect(m_refreshBtn, &QPushButton::clicked, this, &SyncLogView::refresh);
    m_pruneBtn = new QPushButton(tr("Älter 30 Tage löschen"));
    m_pruneBtn->setMinimumWidth(170);
    m_pruneBtn->setMinimumHeight(32);
    connect(m_pruneBtn, &QPushButton::clicked, this, [this]() {
        if (QMessageBox::question(this, tr("Bereinigen"),
                tr("Einträge älter als 30 Tage löschen?"),
                QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
            m_dbSync->pruneLog(30); refresh();
        }
    });
    fbLay->addWidget(m_filterCb, 1);
    fbLay->addWidget(m_refreshBtn);
    fbLay->addWidget(m_pruneBtn);
    logVBox->addWidget(filterBar);

    m_summaryLabel = new QLabel();
    m_summaryLabel->setStyleSheet("color:#555; font-size:10pt; padding:2px 4px;");
    logVBox->addWidget(m_summaryLabel);

    m_logTable = new QTableWidget();
    m_logTable->setColumnCount(7);
    m_logTable->setHorizontalHeaderLabels(
        {tr("Zeitpunkt"), tr("Tabelle"), tr("Aktion"),
         tr("Richtung"), tr("Status"), tr("Seq#"), tr("Gerät")});
    m_logTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_logTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_logTable->setAlternatingRowColors(true);
    m_logTable->setSortingEnabled(true);
    m_logTable->verticalHeader()->setVisible(false);
    {
        QHeaderView *hdr = m_logTable->horizontalHeader();
        hdr->setSectionResizeMode(QHeaderView::Interactive);
        hdr->setDefaultSectionSize(100);
        m_logTable->setColumnWidth(0, 145);
        m_logTable->setColumnWidth(1,  80);
        m_logTable->setColumnWidth(2,  80);
        m_logTable->setColumnWidth(3,  70);
        m_logTable->setColumnWidth(4,  90);
        m_logTable->setColumnWidth(5,  55);
        m_logTable->setColumnWidth(6, 180);
        hdr->setStretchLastSection(true);
        // Sortierung über Refreshes hinweg merken
        connect(hdr, &QHeaderView::sortIndicatorChanged,
                this, [this](int col, Qt::SortOrder order) {
                    m_logSortCol   = col;
                    m_logSortOrder = order;
                });
    }
    logVBox->addWidget(m_logTable, 1);
    m_tabs->addTab(logWidget, tr("Änderungslog"));

    // ── Tab 2: Knowledge Matrix ────────────────────────────────────────────
    auto *matrixWidget = new QWidget();
    auto *matrixVBox   = new QVBoxLayout(matrixWidget);
    matrixVBox->setContentsMargins(4, 4, 4, 4);
    matrixVBox->setSpacing(4);

    auto *matrixInfo = new QLabel(
        tr("Zeigt für jeden bekannten Peer, bis zu welcher Sequenznummer "
           "er Änderungen von welchem Ursprungsgerät erhalten hat."));
    matrixInfo->setWordWrap(true);
    matrixInfo->setStyleSheet("color:#555; font-size:10pt; padding:2px 4px;");
    matrixVBox->addWidget(matrixInfo);

    m_matrixSummaryLabel = new QLabel();
    m_matrixSummaryLabel->setStyleSheet(
        "color:#1F4E79; font-size:10pt; font-weight:bold; padding:2px 4px;");
    matrixVBox->addWidget(m_matrixSummaryLabel);

    m_matrixTable = new QTableWidget();
    m_matrixTable->setColumnCount(4);
    m_matrixTable->setHorizontalHeaderLabels(
        {tr("Quelle (Ursprung)"), tr("Ziel (Peer)"),
         tr("Seq (Q/Z)"), tr("Status")});
    m_matrixTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_matrixTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_matrixTable->setAlternatingRowColors(true);
    m_matrixTable->setSortingEnabled(true);
    m_matrixTable->verticalHeader()->setVisible(false);
    {
        QHeaderView *hdr = m_matrixTable->horizontalHeader();
        hdr->setSectionResizeMode(QHeaderView::Interactive);
        hdr->setDefaultSectionSize(120);
        m_matrixTable->setColumnWidth(0, 200);
        m_matrixTable->setColumnWidth(1, 200);
        m_matrixTable->setColumnWidth(2,  90);
        m_matrixTable->setColumnWidth(3,  90);
        hdr->setStretchLastSection(false);
        // Sortierung über Refreshes hinweg merken
        connect(hdr, &QHeaderView::sortIndicatorChanged,
                this, [this](int col, Qt::SortOrder order) {
                    m_matSortCol   = col;
                    m_matSortOrder = order;
                });
    }
    matrixVBox->addWidget(m_matrixTable, 1);

    auto *matrixLegend = new QLabel(
        tr("Tooltip: Vollständige Geräte-ID  |  "
           "Grün: Sync vollständig  |  Orange: Einträge noch ausstehend  |  "
           "Blau kursiv: Dieses Gerät (\u24c1)  |  Seq = Max-Seq der Quelle / bekannte Seq des Ziels"));
    matrixLegend->setStyleSheet("color:#888; font-size:9pt; padding:2px 4px;");
    matrixLegend->setWordWrap(true);
    matrixVBox->addWidget(matrixLegend);

    m_tabs->addTab(matrixWidget, tr("Knowledge Matrix"));

    // Schließen-Button
    auto *closeBtn = new QPushButton(tr("Schließen"), this);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    auto *btnRow = new QHBoxLayout();
    btnRow->setContentsMargins(0, 4, 0, 0);
    btnRow->addStretch();
    btnRow->addWidget(closeBtn);
    vbox->addLayout(btnRow);

    // Tab-Wechsel: Matrix bei Bedarf neu laden
    connect(m_tabs, &QTabWidget::currentChanged, this, [this](int idx) {
        if (idx == 1) refreshMatrix();
    });

    // Geometrie beim Schließen speichern
#if defined(Q_OS_WIN)
    connect(this, &QDialog::finished, this, [this](int) {
        QSettings s;
        s.setValue(QStringLiteral("synclog/geometry"), saveGeometry());
        s.setValue(QStringLiteral("synclog/log_header"),
                   m_logTable->horizontalHeader()->saveState());
        s.setValue(QStringLiteral("synclog/matrix_header"),
                   m_matrixTable->horizontalHeader()->saveState());
    });
#endif
}
#endif // !Q_OS_ANDROID

#if defined(Q_OS_ANDROID)
#include "synclogview.moc"
#endif

// ── Auto-Refresh: Timer starten wenn sichtbar, stoppen wenn verborgen ────────

void SyncLogView::showEvent(QShowEvent *e)
{
    QDialog::showEvent(e);
    if (m_refreshTimer) m_refreshTimer->start();
}

void SyncLogView::hideEvent(QHideEvent *e)
{
    QDialog::hideEvent(e);
    if (m_refreshTimer) m_refreshTimer->stop();
}
