#include "onenetclient.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStringList>
#include <QUrl>

OneNetClient::OneNetClient(QObject *parent)
    : QObject(parent)
{
    m_mockState = mockTelemetry();
    m_bridgeRestartTimer.setSingleShot(true);

    connect(&m_bridge, &QProcess::readyReadStandardOutput,
            this, &OneNetClient::readBridgeOutput);
    connect(&m_bridge, &QProcess::readyReadStandardError,
            this, &OneNetClient::readBridgeErrors);
    connect(&m_bridge,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus status) {
        if (!m_config.mockMode && !m_stopping) {
            emit bridgeStateChanged(false,
                    QStringLiteral("数据接收服务已停止，%1 秒后重试")
                    .arg(qMax(1, m_config.bridgeRestartIntervalMs / 1000)));
            emit logMessage(QStringLiteral("Bridge 退出码 %1，状态 %2")
                            .arg(exitCode).arg(int(status)));
            m_bridgeRestartTimer.start(
                        qMax(1000, m_config.bridgeRestartIntervalMs));
        }
    });
    connect(&m_bridge, &QProcess::errorOccurred, this,
            [this](QProcess::ProcessError) {
        if (!m_config.mockMode && !m_stopping) {
            emit bridgeStateChanged(false,
                    QStringLiteral("无法启动数据接收服务: ")
                    + m_bridge.errorString());
            m_bridgeRestartTimer.start(
                        qMax(1000, m_config.bridgeRestartIntervalMs));
        }
    });
    connect(&m_bridgeRestartTimer, &QTimer::timeout,
            this, &OneNetClient::startBridge);
}

OneNetClient::~OneNetClient()
{
    stopBridge();
}

OneNetConfig OneNetClient::config() const
{
    return m_config;
}

void OneNetClient::setConfig(const OneNetConfig &config)
{
    m_config = config;
}

bool OneNetClient::loadConfig(const QString &path)
{
    if (!QFileInfo::exists(path))
        return false;

    QSettings settings(path, QSettings::IniFormat);
    OneNetConfig next;
    next.productId = settings.value("onenet/productId", next.productId).toString();
    next.deviceName = settings.value("onenet/deviceName", next.deviceName).toString();
    next.productAccessKey = settings.value("onenet/productAccessKey").toString();
    next.controlApiUrl = settings.value(
                "onenet/controlApiUrl", next.controlApiUrl).toString();
    next.tokenLifetimeSeconds = settings.value(
                "onenet/tokenLifetimeSeconds", 3600).toInt();
    next.javaPath = settings.value("bridge/javaPath", "java").toString();
    next.bridgeJar = settings.value(
                "bridge/jar", "bridge/verticare-bridge.jar").toString();
    next.bridgeConfig = settings.value(
                "bridge/config", "bridge/bridge.properties").toString();
    next.bridgeRestartIntervalMs = settings.value(
                "bridge/restartIntervalMs", 5000).toInt();
    next.pollIntervalMs = settings.value("ui/pollIntervalMs", 3000).toInt();
    next.dataStaleTimeoutMs = settings.value(
                "ui/dataStaleTimeoutMs", 15000).toInt();
    next.controlTimeoutMs = settings.value(
                "ui/controlTimeoutMs", 10000).toInt();
    next.mockMode = settings.value("ui/mockMode", true).toBool();
    setConfig(next);
    return true;
}

bool OneNetClient::saveConfig(const QString &path) const
{
    QSettings settings(path, QSettings::IniFormat);
    settings.setValue("onenet/productId", m_config.productId);
    settings.setValue("onenet/deviceName", m_config.deviceName);
    settings.setValue("onenet/productAccessKey", m_config.productAccessKey);
    settings.setValue("onenet/controlApiUrl", m_config.controlApiUrl);
    settings.setValue("onenet/tokenLifetimeSeconds", m_config.tokenLifetimeSeconds);
    settings.setValue("bridge/javaPath", m_config.javaPath);
    settings.setValue("bridge/jar", m_config.bridgeJar);
    settings.setValue("bridge/config", m_config.bridgeConfig);
    settings.setValue("bridge/restartIntervalMs",
                      m_config.bridgeRestartIntervalMs);
    settings.setValue("ui/pollIntervalMs", m_config.pollIntervalMs);
    settings.setValue("ui/dataStaleTimeoutMs", m_config.dataStaleTimeoutMs);
    settings.setValue("ui/controlTimeoutMs", m_config.controlTimeoutMs);
    settings.setValue("ui/mockMode", m_config.mockMode);
    settings.sync();
    return settings.status() == QSettings::NoError;
}

void OneNetClient::startBridge()
{
    m_stopping = false;
    if (m_config.mockMode) {
        emit telemetryReceived(m_mockState);
        emit bridgeStateChanged(true, QStringLiteral("演示数据"));
        return;
    }

    if (m_bridge.state() != QProcess::NotRunning)
        return;

    const QString jar = resolvePath(m_config.bridgeJar);
    const QString bridgeConfig = resolvePath(m_config.bridgeConfig);
    if (!QFileInfo::exists(jar)) {
        emit bridgeStateChanged(false, QStringLiteral("找不到接收服务: ") + jar);
        return;
    }
    if (!QFileInfo::exists(bridgeConfig)) {
        emit bridgeStateChanged(false, QStringLiteral("找不到接收配置: ") + bridgeConfig);
        return;
    }

    m_stdoutBuffer.clear();
    m_bridge.setProgram(m_config.javaPath);
    m_bridge.setArguments(QStringList() << "-jar" << jar << bridgeConfig);
    m_bridge.setWorkingDirectory(QFileInfo(jar).absolutePath());
    m_bridge.start();
    emit bridgeStateChanged(false, QStringLiteral("正在连接 OneNet 数据服务"));
}

void OneNetClient::stopBridge()
{
    m_stopping = true;
    m_bridgeRestartTimer.stop();
    if (m_bridge.state() == QProcess::NotRunning)
        return;
    m_bridge.terminate();
    if (!m_bridge.waitForFinished(1500)) {
        m_bridge.kill();
        m_bridge.waitForFinished(1000);
    }
}

void OneNetClient::queryProperties()
{
    if (m_config.mockMode) {
        emit telemetryReceived(m_mockState);
        return;
    }
    startBridge();
}

void OneNetClient::setProperties(const QJsonObject &params)
{
    if (m_config.mockMode) {
        applyMockCommand(params);
        emit telemetryReceived(m_mockState);
        emit controlFinished(true, QStringLiteral("演示指令已执行"));
        return;
    }

    if (m_config.productAccessKey.trimmed().isEmpty()) {
        emit controlFinished(false, QStringLiteral("未配置产品 Access Key"));
        return;
    }

    QJsonObject body;
    body.insert("product_id", m_config.productId);
    body.insert("device_name", m_config.deviceName);
    body.insert("params", params);

    QNetworkRequest request(QUrl(m_config.controlApiUrl));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", generateAuthorization().toUtf8());

    QNetworkReply *reply = m_network.post(
                request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    QTimer::singleShot(qMax(1000, m_config.controlTimeoutMs), reply, [reply]() {
        if (reply->isRunning())
            reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QByteArray bytes = reply->readAll();
        const QJsonDocument document = QJsonDocument::fromJson(bytes);
        const QJsonObject root = document.object();
        const bool networkOk = reply->error() == QNetworkReply::NoError;
        const bool apiOk = networkOk && root.value("code").toInt(-1) == 0;
        const QJsonObject data = root.value("data").toObject();
        const bool deviceOk = apiOk && data.value("code").toInt(-1) == 200;

        QString message;
        if (deviceOk) {
            message = QStringLiteral("设备已执行控制指令");
        } else if (apiOk) {
            message = QStringLiteral("设备执行失败: ")
                    + data.value("msg").toString(QStringLiteral("无响应"));
        } else if (!networkOk) {
            message = reply->error() == QNetworkReply::OperationCanceledError
                    ? QStringLiteral("控制请求超时")
                    : reply->errorString();
        } else {
            message = root.value("msg").toString(QStringLiteral("控制请求失败"));
        }

        emit logMessage(QString::fromUtf8(bytes));
        emit controlFinished(deviceOk, message);
        reply->deleteLater();
    });
}

void OneNetClient::readBridgeOutput()
{
    m_stdoutBuffer.append(m_bridge.readAllStandardOutput());
    int newline = -1;
    while ((newline = m_stdoutBuffer.indexOf('\n')) >= 0) {
        const QByteArray line = m_stdoutBuffer.left(newline).trimmed();
        m_stdoutBuffer.remove(0, newline + 1);
        if (line.isEmpty())
            continue;

        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(line, &error);
        if (error.error != QJsonParseError::NoError || !document.isObject()) {
            emit logMessage(QStringLiteral("接收服务输出无法解析: ")
                            + QString::fromUtf8(line));
            continue;
        }

        const QJsonObject root = document.object();
        if (root.value("type").toString() != "telemetry")
            continue;
        const QJsonObject params = root.value("params").toObject();
        const QJsonObject telemetry = normalizeTelemetry(params);
        if (!telemetry.isEmpty()) {
            emit telemetryReceived(telemetry);
            emit bridgeStateChanged(true, QStringLiteral("OneNet 实时数据"));
        }
    }
}

void OneNetClient::readBridgeErrors()
{
    const QString text = QString::fromLocal8Bit(
                m_bridge.readAllStandardError()).trimmed();
    if (!text.isEmpty()) {
        emit logMessage(text);
        if (text.contains("VertiCare bridge connected:"))
            emit bridgeStateChanged(true, QStringLiteral("OneNet 数据服务已连接"));
        else if (text.contains("Bridge connection failed:"))
            emit bridgeStateChanged(false, QStringLiteral("OneNet 数据服务重连中"));
    }
}

QString OneNetClient::resolvePath(const QString &path) const
{
    QFileInfo info(path);
    if (info.isAbsolute())
        return info.absoluteFilePath();
    return QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(path);
}

QString OneNetClient::generateAuthorization() const
{
    const QString version = "2022-05-01";
    const QString method = "sha1";
    const QString resource = "products/" + m_config.productId;
    const qint64 expires = QDateTime::currentSecsSinceEpoch()
            + qMax(60, m_config.tokenLifetimeSeconds);
    const QByteArray stringToSign = QString("%1\n%2\n%3\n%4")
            .arg(expires).arg(method, resource, version).toUtf8();
    const QByteArray decodedKey =
            QByteArray::fromBase64(m_config.productAccessKey.toUtf8());
    const QByteArray signature = hmacSha1(decodedKey, stringToSign).toBase64();

    return QString("version=%1&res=%2&et=%3&method=%4&sign=%5")
            .arg(version,
                 QString::fromUtf8(QUrl::toPercentEncoding(resource)),
                 QString::number(expires),
                 method,
                 QString::fromUtf8(QUrl::toPercentEncoding(signature)));
}

QByteArray OneNetClient::hmacSha1(const QByteArray &key,
                                  const QByteArray &message) const
{
    const int blockSize = 64;
    QByteArray normalized = key;
    if (normalized.size() > blockSize)
        normalized = QCryptographicHash::hash(normalized, QCryptographicHash::Sha1);
    normalized = normalized.leftJustified(blockSize, '\0', true);

    QByteArray outer(blockSize, char(0x5c));
    QByteArray inner(blockSize, char(0x36));
    for (int i = 0; i < blockSize; ++i) {
        outer[i] = char(outer.at(i) ^ normalized.at(i));
        inner[i] = char(inner.at(i) ^ normalized.at(i));
    }
    const QByteArray innerHash = QCryptographicHash::hash(
                inner + message, QCryptographicHash::Sha1);
    return QCryptographicHash::hash(
                outer + innerHash, QCryptographicHash::Sha1);
}

QJsonObject OneNetClient::normalizeTelemetry(const QJsonObject &source) const
{
    QJsonObject telemetry;
    const QStringList keys = {
        "temperature", "airHumidity", "lightValue", "lightStatus",
        "rainDetected", "vibrationDetected", "maintenanceEvent",
        "irrigationState", "servoAngle", "controlMode",
        "manualIrrigation", "openThreshold", "closeThreshold",
        "dhtHealthy", "rainSensorHealthy", "sensorHealthy"
    };
    for (const QString &key : keys) {
        if (source.contains(key))
            telemetry.insert(key, unwrapValue(source.value(key)));
    }
    return telemetry;
}

QJsonValue OneNetClient::unwrapValue(const QJsonValue &value) const
{
    if (value.isObject()) {
        const QJsonObject object = value.toObject();
        if (object.contains("value"))
            return object.value("value");
    }
    return value;
}

QJsonObject OneNetClient::mockTelemetry() const
{
    QJsonObject object;
    object.insert("temperature", 26.8);
    object.insert("airHumidity", 42.0);
    object.insert("lightValue", 2400);
    object.insert("lightStatus", "normal");
    object.insert("rainDetected", false);
    object.insert("vibrationDetected", false);
    object.insert("maintenanceEvent", false);
    object.insert("irrigationState", false);
    object.insert("servoAngle", 0);
    object.insert("controlMode", 0);
    object.insert("manualIrrigation", false);
    object.insert("openThreshold", 45.0);
    object.insert("closeThreshold", 60.0);
    object.insert("dhtHealthy", true);
    object.insert("rainSensorHealthy", true);
    object.insert("sensorHealthy", true);
    return object;
}

void OneNetClient::applyMockCommand(const QJsonObject &params)
{
    for (auto it = params.begin(); it != params.end(); ++it)
        m_mockState.insert(it.key(), it.value());
    if (m_mockState.value("controlMode").toInt() == 1) {
        const bool open = m_mockState.value("manualIrrigation").toBool();
        m_mockState.insert("irrigationState", open);
        m_mockState.insert("servoAngle", open ? 90 : 0);
    }
}
