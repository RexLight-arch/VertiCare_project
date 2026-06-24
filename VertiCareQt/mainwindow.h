#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "onenetclient.h"

#include <QJsonObject>
#include <QLabel>
#include <QMainWindow>
#include <QMap>
#include <QTimer>

class QCheckBox;
class QDoubleSpinBox;
class QRadioButton;
class PlantSceneWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void refreshTelemetry();
    void sendControl();
    void applyTelemetry(const QJsonObject &telemetry);

private:
    void buildUi();
    QWidget *buildMetricCard(const QString &key, const QString &name,
                             const QString &unit, const QString &accent);
    QWidget *buildControlPanel();
    void setMetric(const QString &key, const QString &value);
    void setConnectionState(bool online, const QString &message);
    QJsonObject controlParamsFromUi() const;
    QString configPath() const;

private:
    OneNetClient m_client;
    QTimer m_pollTimer;
    QMap<QString, QLabel *> m_metrics;

    PlantSceneWidget *m_plantScene = nullptr;
    QLabel *m_connectionDot = nullptr;
    QLabel *m_connectionText = nullptr;
    QLabel *m_updatedAt = nullptr;
    QLabel *m_modeBadge = nullptr;

    QRadioButton *m_autoMode = nullptr;
    QRadioButton *m_manualMode = nullptr;
    QCheckBox *m_manualIrrigation = nullptr;
    QDoubleSpinBox *m_openThreshold = nullptr;
    QDoubleSpinBox *m_closeThreshold = nullptr;
};

#endif
