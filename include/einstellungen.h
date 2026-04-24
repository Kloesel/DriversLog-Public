#pragma once
#include <QWidget>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include "database.h"
#include "syncmanager.h"
#if defined(Q_OS_ANDROID)
#  include <QQuickWidget>
class EinstellungenBridge;   // forward — definiert in einstellungen.cpp
#endif

class EinstellungenWidget : public QWidget
{
    Q_OBJECT
public:
    explicit EinstellungenWidget(Database *db, SyncManager *syncMgr,
                                 QWidget *parent = nullptr);
    void load();
    void save();
#if defined(Q_OS_ANDROID)
    void setOrsUnlocked();
#endif

signals:
    void settingsChanged();
    void cancelRequested();

private slots:
    void onSave();
    void onCancel();
#if !defined(Q_OS_ANDROID)
    void onSyncModeChanged(const QString &mode);
    void onChooseDbPath();
    void onMehrereFahrerChanged(Qt::CheckState state);
#endif

private:
#if !defined(Q_OS_ANDROID)
    void updateWifiWidgetsVisibility(bool visible);
#endif

    Database    *m_db;
    SyncManager *m_syncMgr = nullptr;

#if defined(Q_OS_ANDROID)
    QQuickWidget       *m_qmlView  = nullptr;
    EinstellungenBridge *m_bridge  = nullptr;
#else
    // Desktop-Widgets
    QLineEdit  *m_dbPathEdit;
    QComboBox  *m_monatCb;
    QSpinBox   *m_jahrSpin;
    QComboBox  *m_standardAdresseCb;
    QComboBox  *m_standardFahrerCb;
    QCheckBox  *m_hinZurueckCb;
    QCheckBox  *m_mehrereFahrerCb;
    QLabel     *m_stdFahrerLabel;
    QComboBox  *m_syncModeCb;
    QLineEdit  *m_apiKeyEdit;
    QGroupBox  *m_wifiGroup   = nullptr;
    QSpinBox   *m_udpPortSpin  = nullptr;
    QSpinBox   *m_tcpPortSpin  = nullptr;
    QComboBox  *m_languageCb   = nullptr;
    QLabel     *m_langRestartLabel = nullptr;
#endif
};
