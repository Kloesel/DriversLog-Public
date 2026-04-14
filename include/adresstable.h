#pragma once
#include <QWidget>
#include <QTableWidget>
#include <QPushButton>
#include "database.h"
#include "syncmanager.h"
#include "models.h"
#include <QResizeEvent>
#if defined(Q_OS_ANDROID)
#  include <QQuickWidget>
#  include <functional>
class AdressListBridge;   // forward — definiert in adresstable.cpp
#endif

class AdressTable : public QWidget
{
    Q_OBJECT
public:
    QTableWidget *tableWidget() const { return m_table; }
    explicit AdressTable(Database *db, SyncManager *syncMgr, QWidget *parent = nullptr);
    void refresh();
    bool hasUnsavedChanges() const { return false; }

#if defined(Q_OS_ANDROID)
    void openNewAdressDialog(std::function<void(int)> onSaved = {});

    static void showNewAdressDialog(Database *db, QWidget *parentWidget,
                                    std::function<void(int)> onSaved);

    // Source-Swap direkt auf qmlList; onClose steuert Rückkehr statt AdressListView.
    static void runAdressQmlView(Adresse *adresse, bool isNew,
                                 Database *db, QWidget *parentWidget,
                                 std::function<void(int)> onSaved,
                                 std::function<void()>   onDeleted,
                                 std::function<void(const QString&,int)> showSnackbar,
                                 QQuickWidget *qmlList = nullptr,
                                 std::function<void()> onClose = {});
#endif

signals:
    void dataChanged();
    void savedToDb();

private slots:
    void onAdd();
    void onEdit();
    void onDelete();
    void onImportCsv();

protected:
    void resizeEvent(QResizeEvent *e) override;

private:
    void populateTable(const QVector<Adresse> &list);
    void openDialog(Adresse *adresse);

#if defined(Q_OS_ANDROID)
    void openAndroidQmlDialog(Adresse *adresse,
                              std::function<void(int)> onSaved = {});
#endif

    Database     *m_db;
    SyncManager  *m_syncMgr;

    QTableWidget *m_table     = nullptr;
    QPushButton  *m_addBtn    = nullptr;
    QPushButton  *m_editBtn   = nullptr;
    QPushButton  *m_delBtn    = nullptr;
    QPushButton  *m_importBtn = nullptr;

#if defined(Q_OS_ANDROID)
    QQuickWidget     *m_qmlList    = nullptr;
    AdressListBridge *m_listBridge = nullptr;
#endif

    int           m_lastSortColumn = -1;
    Qt::SortOrder m_lastSortOrder  = Qt::AscendingOrder;

    enum Col { C_BEZ=0, C_STR, C_HNR, C_PLZ, C_ORT, C_COUNT };
};
