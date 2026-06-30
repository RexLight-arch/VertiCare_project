#include "telemetrystore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSqlError>
#include <QSqlQuery>
#include <QTextStream>
#include <QUuid>

bool TelemetryStore::open(const QString &databasePath)
{
    const QFileInfo info(databasePath);
    QDir().mkpath(info.absolutePath());

    const QString connectionName = QStringLiteral("verticare_%1")
            .arg(QUuid::createUuid().toString(QUuid::Id128));
    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    m_db.setDatabaseName(databasePath);
    if (!m_db.open()) {
        m_lastError = m_db.lastError().text();
        return false;
    }
    return ensureSchema();
}

bool TelemetryStore::isOpen() const
{
    return m_db.isOpen();
}

QString TelemetryStore::lastError() const
{
    return m_lastError;
}

bool TelemetryStore::ensureSchema()
{
    QSqlQuery query(m_db);
    const char *telemetrySql =
            "CREATE TABLE IF NOT EXISTS telemetry ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "created_at TEXT NOT NULL,"
            "temperature REAL,"
            "humidity REAL,"
            "light_raw INTEGER,"
            "brightness INTEGER,"
            "air_quality INTEGER,"
            "rain INTEGER,"
            "maintenance INTEGER,"
            "safety INTEGER,"
            "irrigation INTEGER,"
            "operator_id TEXT,"
            "payload TEXT NOT NULL)";
    if (!query.exec(QString::fromLatin1(telemetrySql))) {
        m_lastError = query.lastError().text();
        return false;
    }

    const char *eventSql =
            "CREATE TABLE IF NOT EXISTS events ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "created_at TEXT NOT NULL,"
            "type TEXT,"
            "level TEXT,"
            "message TEXT)";
    if (!query.exec(QString::fromLatin1(eventSql))) {
        m_lastError = query.lastError().text();
        return false;
    }
    return true;
}

bool TelemetryStore::saveTelemetry(const QJsonObject &telemetry)
{
    if (!isOpen())
        return false;

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "INSERT INTO telemetry (created_at, temperature, humidity, light_raw, "
        "brightness, air_quality, rain, maintenance, safety, irrigation, "
        "operator_id, payload) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));

    const int lightRaw = telemetry.value(QStringLiteral("lightValue")).toInt();
    const bool maintenance = telemetry.value(QStringLiteral("vibrationDetected")).toBool() ||
            telemetry.value(QStringLiteral("maintenanceEvent")).toBool();
    query.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    query.addBindValue(telemetry.value(QStringLiteral("temperature")).toDouble());
    query.addBindValue(telemetry.value(QStringLiteral("airHumidity")).toDouble());
    query.addBindValue(lightRaw);
    query.addBindValue(brightnessPercent(lightRaw));
    query.addBindValue(telemetry.value(QStringLiteral("airQualityPercent")).toInt());
    query.addBindValue(telemetry.value(QStringLiteral("rainDetected")).toBool() ? 1 : 0);
    query.addBindValue(maintenance ? 1 : 0);
    query.addBindValue(telemetry.value(QStringLiteral("safetyAlert")).toBool() ? 1 : 0);
    query.addBindValue(telemetry.value(QStringLiteral("irrigationState")).toBool() ? 1 : 0);
    query.addBindValue(telemetry.value(QStringLiteral("currentOperatorId")).toString());
    query.addBindValue(QString::fromUtf8(QJsonDocument(telemetry).toJson(QJsonDocument::Compact)));

    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return false;
    }
    return true;
}

bool TelemetryStore::saveEvent(const QString &type, const QString &level,
                               const QString &message)
{
    if (!isOpen())
        return false;

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "INSERT INTO events (created_at, type, level, message) VALUES (?, ?, ?, ?)"));
    query.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    query.addBindValue(type);
    query.addBindValue(level);
    query.addBindValue(message);
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return false;
    }
    return true;
}

bool TelemetryStore::loadLastTelemetry(QJsonObject *telemetry) const
{
    if (!telemetry || !isOpen())
        return false;

    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral(
            "SELECT payload FROM telemetry ORDER BY id DESC LIMIT 1"))) {
        return false;
    }
    if (!query.next())
        return false;

    const QJsonDocument document = QJsonDocument::fromJson(
                query.value(0).toString().toUtf8());
    if (!document.isObject())
        return false;
    *telemetry = document.object();
    return true;
}

QVector<TrendSample> TelemetryStore::loadRecentSamples(int minutes, int limit) const
{
    QVector<TrendSample> samples;
    if (!isOpen())
        return samples;

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT created_at, temperature, humidity, brightness, air_quality "
        "FROM telemetry WHERE created_at >= ? ORDER BY id ASC LIMIT ?"));
    query.addBindValue(QDateTime::currentDateTime().addSecs(-minutes * 60).toString(Qt::ISODate));
    query.addBindValue(limit);
    if (!query.exec())
        return samples;

    while (query.next()) {
        TrendSample sample;
        sample.timestamp = QDateTime::fromString(query.value(0).toString(), Qt::ISODate);
        sample.temperature = query.value(1).toDouble();
        sample.humidity = query.value(2).toDouble();
        sample.brightness = query.value(3).toDouble();
        sample.airQuality = query.value(4).toDouble();
        samples.append(sample);
    }
    return samples;
}

QStringList TelemetryStore::loadRecentEvents(int limit) const
{
    QStringList events;
    if (!isOpen())
        return events;

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT created_at, level, message FROM events ORDER BY id DESC LIMIT ?"));
    query.addBindValue(limit);
    if (!query.exec())
        return events;
    while (query.next()) {
        const QDateTime time = QDateTime::fromString(query.value(0).toString(), Qt::ISODate);
        events << QStringLiteral("%1  %2  %3")
                  .arg(time.toString(QStringLiteral("MM-dd HH:mm:ss")),
                       query.value(1).toString(),
                       query.value(2).toString());
    }
    return events;
}

bool TelemetryStore::exportCsv(const QString &path, int limit) const
{
    if (!isOpen())
        return false;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream out(&file);
    out.setCodec("UTF-8");
    out << "time,temperature,humidity,brightness,air_quality,rain,maintenance,"
           "safety,irrigation,operator\n";

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT created_at, temperature, humidity, brightness, air_quality, "
        "rain, maintenance, safety, irrigation, operator_id "
        "FROM telemetry ORDER BY id DESC LIMIT ?"));
    query.addBindValue(limit);
    if (!query.exec())
        return false;

    while (query.next()) {
        QStringList row;
        for (int i = 0; i < 10; ++i) {
            QString cell = query.value(i).toString();
            cell.replace('"', "\"\"");
            row << QStringLiteral("\"%1\"").arg(cell);
        }
        out << row.join(',') << "\n";
    }
    return true;
}

int TelemetryStore::brightnessPercent(int raw)
{
    const int brightRaw = 1200;
    const int darkRaw = 3200;
    return qBound(0, qRound((darkRaw - raw) * 100.0 / (darkRaw - brightRaw)), 100);
}
