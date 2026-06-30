#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "onenetclient.h"
#include "telemetrystore.h"

#include <QJsonObject>
#include <QDateTime>
#include <QLabel>
#include <QMainWindow>
#include <QMap>
#include <QTimer>

class QCheckBox;
class QDoubleSpinBox;
class QLineEdit;
class QListWidget;
class QRadioButton;
class QPushButton;
class QSpinBox;
class PlantSceneWidget;
class TrendChartWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void refreshTelemetry();
    void sendControl();
    void applyTelemetry(const QJsonObject &telemetry);
    void updateFreshness();
    void handleControlFinished(bool ok, const QString &message);
    void exportCsv();
    void saveSettings();

private:
    void buildUi();
    QWidget *buildMetricCard(const QString &key, const QString &name,
                             const QString &unit, const QString &accent);
    QWidget *buildControlPanel();
    QWidget *buildEventPage();
    QWidget *buildTrendPage();
    QWidget *buildSettingsPage();
    void loadSettingsToUi();
    void initializeStore();
    void reloadTrendPage();
    void appendEvent(const QString &type, const QString &level,
                     const QString &message, bool persist = true);
    void setMetric(const QString &key, const QString &value);
    void setConnectionState(bool online, const QString &message,
                            const QString &color = QString());
    void setControlBusy(bool busy);
    QJsonObject controlParamsFromUi() const;
    QString configPath() const;

private:
    OneNetClient m_client;
    TelemetryStore m_store;
    QTimer m_pollTimer;
    QTimer m_healthTimer;
    QMap<QString, QLabel *> m_metrics;

    PlantSceneWidget *m_plantScene = nullptr;
    QLabel *m_connectionDot = nullptr;
    QLabel *m_connectionText = nullptr;
    QLabel *m_updatedAt = nullptr;
    QLabel *m_modeBadge = nullptr;
    QLabel *m_commandStatus = nullptr;
    QListWidget *m_eventList = nullptr;
    TrendChartWidget *m_trendChart = nullptr;
    QLabel *m_storeStatus = nullptr;

    QRadioButton *m_autoMode = nullptr;
    QRadioButton *m_manualMode = nullptr;
    QCheckBox *m_manualIrrigation = nullptr;
    QDoubleSpinBox *m_openThreshold = nullptr;
    QDoubleSpinBox *m_closeThreshold = nullptr;
    QPushButton *m_applyButton = nullptr;
    QPushButton *m_openButton = nullptr;
    QPushButton *m_closeButton = nullptr;
    QLineEdit *m_productIdEdit = nullptr;
    QLineEdit *m_deviceNameEdit = nullptr;
    QLineEdit *m_accessKeyEdit = nullptr;
    QLineEdit *m_javaPathEdit = nullptr;
    QLineEdit *m_bridgeJarEdit = nullptr;
    QLineEdit *m_bridgeConfigEdit = nullptr;
    QSpinBox *m_pollIntervalEdit = nullptr;
    QSpinBox *m_staleTimeoutEdit = nullptr;
    QSpinBox *m_controlTimeoutEdit = nullptr;
    QSpinBox *m_bridgeRestartEdit = nullptr;
    QCheckBox *m_mockModeEdit = nullptr;
    QCheckBox *m_autoStartBridgeEdit = nullptr;
    QLabel *m_settingsStatus = nullptr;
    QDateTime m_lastTelemetryAt;
    int m_lastEventSequence = 0;
    bool m_sensorHealthy = true;
    bool m_safetyAlert = false;
};

#endif
