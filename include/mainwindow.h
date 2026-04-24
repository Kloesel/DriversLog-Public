#pragma once
#include <QMainWindow>
#include <QTabWidget>
#include <QLabel>
#include <QStatusBar>
#include <QTimer>
#include "database.h"
#include "fahrtentable.h"
#include "adresstable.h"
#include "fahrertable.h"
#include "einstellungen.h"
#include "datenexport.h"
#include "syncmanager.h"
#include "synclogview.h"
#include "distanceservice.h"
#include "version.h"
#include "swipefilter.h"
#include "drawerwidget.h"
#if defined(Q_OS_ANDROID)
#  include <QQuickWidget>
#  include <QObject>
#  include "drawerwidget.h"
class AppBarBridge;
#endif

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onTabChanged(int index);
    void onDataChanged();
    void onSettingsChanged();
    void onAutoSave();
    void onSaveAll();
    void onSyncStart();
    void onSyncStatus();
    void showSyncLog();
#if defined(Q_OS_ANDROID)
    void checkAndRequestNearbyWifiPermission();

    void onDrawerPageSelected(int idx);
    void onBurgerClicked();
    void onDrawerClosed();
    void onApplicationStateChanged(Qt::ApplicationState state);
#endif
    void onSyncFinished(bool ok, const QString &msg);
    void onSyncProgress(const QString &msg);
    void onNavFahrten();
    void onNavAdressen();
    void onNavFahrer();
    void onNavEinstellungen();
    void onNavExport();
    void showAbout();
    void showQtAbout();
    void showHelp();

private:
    void setupStatusBar();
    void applyTheme();

    Database            *m_db;
    DistanceService     *m_distSvc;
    SyncManager         *m_syncMgr;

    QTabWidget          *m_tabs;
    FahrtenTable        *m_fahrtenTab;
    AdressTable         *m_adressTab;
    FahrerTable         *m_fahrerTab;
    EinstellungenWidget *m_einstellTab;
    DatenExport         *m_exportTab;

    QLabel              *m_statusLabel;
    QLabel              *m_syncLabel = nullptr;

#if defined(Q_OS_ANDROID)
    QQuickWidget        *m_appBarQml     = nullptr;
    AppBarBridge        *m_appBarBridge  = nullptr;
    DrawerWidget        *m_drawer        = nullptr;
    SwipeFilter         *m_swipeFilter   = nullptr;
    void setupAndroidNav();
    void setAndroidPage(int index);
    void updateDrawerSelection(int pageIdx);
    void hideAllQuickWidgets();
#endif

    QString m_lastKnownLanguage;           // Sprachstand beim letzten Speichern (alle Plattformen)
    bool m_hasChanges      = false;
    bool m_initialLoadDone = false;

    Ui::MainWindow *ui;
};
