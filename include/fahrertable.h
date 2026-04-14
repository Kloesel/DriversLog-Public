#pragma once
#include <QWidget>
#include <QTableWidget>
#include <QPushButton>
#include "database.h"
#include "syncmanager.h"
#include <QResizeEvent>
#if defined(Q_OS_ANDROID)
#  include <QQuickWidget>
#  include <functional>
class FahrerListBridge;   // forward — definiert in fahrertable.cpp
#endif

class FahrerTable : public QWidget
{
    Q_OBJECT
public:
    QTableWidget *tableWidget() const { return m_table; }
    explicit FahrerTable(Database *db, SyncManager *syncMgr, QWidget *parent = nullptr);
    void refresh();
    bool hasUnsavedChanges() const { return false; }

signals:
    void dataChanged();
    void savedToDb();

#if defined(Q_OS_ANDROID)
public:
    static void runFahrerQmlView(Database *db,
                                 QQuickWidget *qmlList,
                                 std::function<void(int)> onSaved,
                                 std::function<void(const QString&,int)> showSnackbar,
                                 std::function<void()> onClose);
#endif

private slots:
    void onAdd();
    void onEdit();
    void onDelete();

protected:
    void resizeEvent(QResizeEvent *e) override;

private:
    void populateTable(const QVector<Fahrer> &list);
    void openDialog(Fahrer *fahrer);
#if defined(Q_OS_ANDROID)
    void openAndroidQmlDialog(Fahrer *fahrer);
#endif

    Database     *m_db;
    SyncManager  *m_syncMgr;

    // Desktop
    QTableWidget *m_table   = nullptr;
    QPushButton  *m_addBtn  = nullptr;
    QPushButton  *m_editBtn = nullptr;
    QPushButton  *m_delBtn  = nullptr;

#if defined(Q_OS_ANDROID)
    QQuickWidget     *m_qmlList    = nullptr;
    FahrerListBridge *m_listBridge = nullptr;
#endif

    int           m_lastSortColumn = -1;
    Qt::SortOrder m_lastSortOrder  = Qt::AscendingOrder;

    enum Col { C_NAME=0, C_COUNT };
};
