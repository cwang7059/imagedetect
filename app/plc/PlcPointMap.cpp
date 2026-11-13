#include "PlcPointMap.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

namespace {

PlcPoint parsePoint(const QString &name, const QJsonObject &o)
{
    PlcPoint p;
    p.name = name;
    p.db = o.value(QStringLiteral("db")).toInt(100);
    p.offset = o.value(QStringLiteral("offset")).toInt(0);
    p.type = o.value(QStringLiteral("type")).toString(QStringLiteral("bool"));
    p.bit = o.value(QStringLiteral("bit")).toInt(0);
    return p;
}

} // namespace

bool PlcPointMap::loadFromFile(const QString &path, QString *error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("Cannot open point map: %1").arg(path);
        return false;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) {
        if (error)
            *error = QStringLiteral("Invalid point map JSON");
        return false;
    }
    const QJsonObject root = doc.object();
    const QJsonObject conn = root.value(QStringLiteral("connection")).toObject();
    m_conn.protocol = conn.value(QStringLiteral("protocol")).toString(QStringLiteral("s7"));
    m_conn.ip = conn.value(QStringLiteral("ip")).toString(QStringLiteral("127.0.0.1"));
    m_conn.rack = conn.value(QStringLiteral("rack")).toInt(0);
    m_conn.slot = conn.value(QStringLiteral("slot")).toInt(1);
    m_conn.dbNumber = conn.value(QStringLiteral("db_number")).toInt(100);
    m_conn.pollMs = conn.value(QStringLiteral("poll_ms")).toInt(20);
    m_conn.heartbeatMs = conn.value(QStringLiteral("heartbeat_ms")).toInt(500);
    m_conn.dbSizeBytes = root.value(QStringLiteral("db_size_bytes")).toInt(64);

    m_points.clear();
    auto loadGroup = [this](const QJsonObject &group) {
        for (auto it = group.begin(); it != group.end(); ++it) {
            if (!it.value().isObject())
                continue;
            m_points.insert(it.key(), parsePoint(it.key(), it.value().toObject()));
        }
    };
    loadGroup(root.value(QStringLiteral("plc_to_pc")).toObject());
    loadGroup(root.value(QStringLiteral("pc_to_plc")).toObject());

    m_failCodes.clear();
    const QJsonObject fc = root.value(QStringLiteral("fail_codes")).toObject();
    for (auto it = fc.begin(); it != fc.end(); ++it)
        m_failCodes.insert(it.key(), it.value().toString());

    return true;
}

PlcPoint PlcPointMap::point(const QString &name) const
{
    return m_points.value(name);
}

bool PlcPointMap::has(const QString &name) const
{
    return m_points.contains(name);
}

QStringList PlcPointMap::names() const
{
    return m_points.keys();
}
