#pragma once
#include <QDialog>
#include <QTimer>
#include <QTableWidget>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QTabWidget>
#include <QSqlQuery>
#include "databasesync.h"
#if defined(Q_OS_ANDROID)
#  include <QQuickWidget>
class SyncLogBridge;   // forward — definiert in synclogview.cpp
#endif

class SyncLogView : public QDialog
{
    Q_OBJECT
public:
    explicit SyncLogView(DatabaseSync *dbSync, QWidget *parent = nullptr);

protected:
    void showEvent(QShowEvent *e) override;
    void hideEvent(QHideEvent *e) override;

private slots:
    void refresh();
#if !defined(Q_OS_ANDROID)
    void onFilterChanged();
#endif

private:
    // Gemeinsam: 3-Sekunden-Timer für Hintergrundaktualisierung
    QTimer *m_refreshTimer = nullptr;

#if !defined(Q_OS_ANDROID)
    void setupUi();
    void refreshLog();
    void refreshMatrix();

    DatabaseSync    *m_dbSync           = nullptr;
    QTabWidget      *m_tabs             = nullptr;

    // Tab 1: Änderungslog
    QTableWidget    *m_logTable         = nullptr;
    QLabel          *m_summaryLabel     = nullptr;
    QComboBox       *m_filterCb         = nullptr;
    QPushButton     *m_refreshBtn       = nullptr;
    QPushButton     *m_pruneBtn         = nullptr;
    int              m_logSortCol       = 0;                    // Sortierung Log-Tabelle
    Qt::SortOrder    m_logSortOrder     = Qt::DescendingOrder;  // merken über Refreshes

    // Tab 2: Knowledge Matrix
    QTableWidget    *m_matrixTable      = nullptr;
    QLabel          *m_matrixSummaryLabel = nullptr;
    int              m_matSortCol       = 0;                    // Sortierung Matrix
    Qt::SortOrder    m_matSortOrder     = Qt::AscendingOrder;
#else
    DatabaseSync    *m_dbSync           = nullptr;
    QQuickWidget    *m_qml              = nullptr;
    SyncLogBridge   *m_bridge           = nullptr;
#endif
};
