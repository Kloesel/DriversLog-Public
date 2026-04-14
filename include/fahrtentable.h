#pragma once
#include <QWidget>
#include <QTableWidget>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QDateEdit>
#include <QPushButton>
#include <QLabel>
#include "database.h"
#include "distanceservice.h"
#include "syncmanager.h"
#include "models.h"
#include <QVector>
#if defined(Q_OS_ANDROID)
#  include <QDate>
#  include <functional>
#  include <QQuickWidget>
#endif

class FahrtListBridge;   // forward — definiert in fahrtentable.cpp

class FahrtenTable : public QWidget
{
    Q_OBJECT
public:
    QTableWidget *tableWidget() const { return m_table; }
    explicit FahrtenTable(Database *db, DistanceService *distSvc,
                          SyncManager *syncMgr, QWidget *parent = nullptr);

    void refresh();
    bool hasUnsavedChanges() const { return false; }
    void setFahrerVisible(bool visible);

#if defined(Q_OS_ANDROID)
    // public: wird von FahrtFormBridge aufgerufen
    static void showQmlDatePicker(QQuickWidget *qw, QDate current,
                                  std::function<void(QDate)> onAccepted);
#endif

signals:
    void dataChanged();
    void savedToDb();

private slots:
    void onAdd();
    void onEdit();
    void onDelete();

protected:
    void resizeEvent(QResizeEvent *e) override;

private:
    void populateTable(const QVector<Fahrt> &fahrten);
    void openDialog(Fahrt *fahrt);
    QString colToDbField(int col) const;
#if defined(Q_OS_ANDROID)
    void openAndroidQmlDialog(const Fahrt &f, bool isNew,
                              const QVector<Adresse> &adressen,
                              const QVector<Fahrer>  &fahrer,
                              const Einstellungen    &e);
#endif

    Database        *m_db;
    DistanceService *m_distSvc;
    SyncManager     *m_syncMgr;

    // Desktop
    QTableWidget    *m_table  = nullptr;
    QPushButton     *m_addBtn = nullptr;
    QPushButton     *m_editBtn= nullptr;
    QPushButton     *m_delBtn = nullptr;

#if defined(Q_OS_ANDROID)
    // Android: QML-Liste statt QTableWidget
    QQuickWidget    *m_qmlList   = nullptr;
    FahrtListBridge *m_listBridge= nullptr;
#endif

    int           m_lastSortColumn = -1;
    Qt::SortOrder m_lastSortOrder  = Qt::AscendingOrder;

    enum Col { COL_DATUM=0, COL_FAHRER, COL_START, COL_ZIEL,
               COL_DIST, COL_HZ, COL_BEM, COL_COUNT };
};
