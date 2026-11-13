#pragma once

#include <QHash>
#include <QString>
#include <QVariant>

struct PlcPoint {
    QString name;
    int db = 100;
    int offset = 0;
    QString type; // bool, uint16, uint32, real
    int bit = 0;  // for bool
};

struct PlcConnectionConfig {
    QString protocol = QStringLiteral("s7"); // s7 | modbus | mock
    QString ip = QStringLiteral("127.0.0.1");
    int rack = 0;
    int slot = 1;
    int dbNumber = 100;
    int pollMs = 20;
    int heartbeatMs = 500;
    int dbSizeBytes = 64;
};

class PlcPointMap
{
public:
    bool loadFromFile(const QString &path, QString *error = nullptr);

    const PlcConnectionConfig &connection() const { return m_conn; }
    PlcPoint point(const QString &name) const;
    bool has(const QString &name) const;
    QStringList names() const;

    QHash<QString, QString> failCodes() const { return m_failCodes; }

private:
    PlcConnectionConfig m_conn;
    QHash<QString, PlcPoint> m_points;
    QHash<QString, QString> m_failCodes;
};
