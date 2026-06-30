#ifndef ONENETCLIENT_H
#define ONENETCLIENT_H

#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QProcess>
#include <QSettings>
#include <QTimer>

struct OneNetConfig
{
    QString productId = "i9912Q27NU";
    QString deviceName = "verticare_01";
    QString productAccessKey;
    QString controlApiUrl =
            "https://iot-api.heclouds.com/thingmodel/set-device-property";
    QString javaPath = "java";
    QString bridgeJar = "bridge/verticare-bridge.jar";
    QString bridgeConfig = "bridge/bridge.properties";
    int tokenLifetimeSeconds = 3600;
    int pollIntervalMs = 3000;
    int dataStaleTimeoutMs = 15000;
    int bridgeRestartIntervalMs = 5000;
    int controlTimeoutMs = 10000;
    bool mockMode = true;
    bool autoStartBridge = true;
};

class OneNetClient : public QObject
{
    Q_OBJECT

public:
    explicit OneNetClient(QObject *parent = nullptr);
    ~OneNetClient() override;

    OneNetConfig config() const;
    void setConfig(const OneNetConfig &config);
    bool loadConfig(const QString &path);
    bool saveConfig(const QString &path) const;

public slots:
    void startBridge();
    void stopBridge();
    void queryProperties();
    void setProperties(const QJsonObject &params);

signals:
    void telemetryReceived(const QJsonObject &telemetry);
    void logMessage(const QString &message);
    void bridgeStateChanged(bool connected, const QString &message);
    void controlFinished(bool ok, const QString &message);

private:
    void readBridgeOutput();
    void readBridgeErrors();
    QString resolvePath(const QString &path) const;
    QString generateAuthorization() const;
    QByteArray hmacSha1(const QByteArray &key, const QByteArray &message) const;
    QJsonObject normalizeTelemetry(const QJsonObject &source) const;
    QJsonValue unwrapValue(const QJsonValue &value) const;
    QJsonObject mockTelemetry() const;
    void applyMockCommand(const QJsonObject &params);

private:
    OneNetConfig m_config;
    QNetworkAccessManager m_network;
    QProcess m_bridge;
    QTimer m_bridgeRestartTimer;
    QByteArray m_stdoutBuffer;
    QJsonObject m_mockState;
    int m_receivedTelemetryCount = 0;
    bool m_stopping = false;
};

#endif
