#ifndef TELEMETRYSTORE_H
#define TELEMETRYSTORE_H

#include <QDateTime>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QString>
#include <QVector>

struct TrendSample
{
    QDateTime timestamp;
    double temperature = 0.0;
    double humidity = 0.0;
    double brightness = 0.0;
    double airQuality = 0.0;
};

class TelemetryStore
{
public:
    bool open(const QString &databasePath);
    bool isOpen() const;
    QString lastError() const;

    bool saveTelemetry(const QJsonObject &telemetry);
    bool saveEvent(const QString &type, const QString &level,
                   const QString &message);
    bool loadLastTelemetry(QJsonObject *telemetry) const;
    QVector<TrendSample> loadRecentSamples(int minutes, int limit = 600) const;
    QStringList loadRecentEvents(int limit = 50) const;
    bool exportCsv(const QString &path, int limit = 5000) const;

private:
    bool ensureSchema();
    static int brightnessPercent(int raw);

private:
    QSqlDatabase m_db;
    QString m_lastError;
};

#endif
