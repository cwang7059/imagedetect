#include "IPlcClient.h"

#include <cstring>

#include <QtEndian>

namespace {

quint16 getBe16(const QByteArray &b, int off)
{
    if (off + 2 > b.size())
        return 0;
    return qFromBigEndian<quint16>(reinterpret_cast<const uchar *>(b.constData() + off));
}

void setBe16(QByteArray *b, int off, quint16 v)
{
    if (off + 2 > b->size())
        return;
    qToBigEndian(v, reinterpret_cast<uchar *>(b->data() + off));
}

quint32 getBe32(const QByteArray &b, int off)
{
    if (off + 4 > b.size())
        return 0;
    return qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(b.constData() + off));
}

float getBeFloat(const QByteArray &b, int off)
{
    const quint32 u = getBe32(b, off);
    float f = 0.f;
    memcpy(&f, &u, sizeof(f));
    return f;
}

void setBeFloat(QByteArray *b, int off, float v)
{
    quint32 u = 0;
    memcpy(&u, &v, sizeof(u));
    if (off + 4 > b->size())
        return;
    qToBigEndian(u, reinterpret_cast<uchar *>(b->data() + off));
}

} // namespace

bool IPlcClient::readBool(const QString &name, bool *ok)
{
    if (!m_map || !m_map->has(name)) {
        if (ok)
            *ok = false;
        return false;
    }
    const PlcPoint p = m_map->point(name);
    QByteArray buf;
    if (!readDb(p.db, p.offset, 1, &buf) || buf.isEmpty()) {
        if (ok)
            *ok = false;
        return false;
    }
    if (ok)
        *ok = true;
    return (buf.at(0) & (1 << p.bit)) != 0;
}

bool IPlcClient::writeBool(const QString &name, bool value, QString *error)
{
    if (!m_map || !m_map->has(name)) {
        if (error)
            *error = QStringLiteral("Unknown point %1").arg(name);
        return false;
    }
    const PlcPoint p = m_map->point(name);
    QByteArray buf;
    if (!readDb(p.db, p.offset, 1, &buf, error) || buf.isEmpty())
        return false;
    uchar b = static_cast<uchar>(buf.at(0));
    if (value)
        b = static_cast<uchar>(b | (1 << p.bit));
    else
        b = static_cast<uchar>(b & ~(1 << p.bit));
    buf[0] = static_cast<char>(b);
    return writeDb(p.db, p.offset, buf, error);
}

quint16 IPlcClient::readUInt16(const QString &name, bool *ok)
{
    if (!m_map || !m_map->has(name)) {
        if (ok)
            *ok = false;
        return 0;
    }
    const PlcPoint p = m_map->point(name);
    QByteArray buf;
    if (!readDb(p.db, p.offset, 2, &buf) || buf.size() < 2) {
        if (ok)
            *ok = false;
        return 0;
    }
    if (ok)
        *ok = true;
    return getBe16(buf, 0);
}

bool IPlcClient::writeUInt16(const QString &name, quint16 value, QString *error)
{
    if (!m_map || !m_map->has(name)) {
        if (error)
            *error = QStringLiteral("Unknown point %1").arg(name);
        return false;
    }
    const PlcPoint p = m_map->point(name);
    QByteArray buf(2, 0);
    setBe16(&buf, 0, value);
    return writeDb(p.db, p.offset, buf, error);
}

quint32 IPlcClient::readUInt32(const QString &name, bool *ok)
{
    if (!m_map || !m_map->has(name)) {
        if (ok)
            *ok = false;
        return 0;
    }
    const PlcPoint p = m_map->point(name);
    QByteArray buf;
    if (!readDb(p.db, p.offset, 4, &buf) || buf.size() < 4) {
        if (ok)
            *ok = false;
        return 0;
    }
    if (ok)
        *ok = true;
    return getBe32(buf, 0);
}

float IPlcClient::readReal(const QString &name, bool *ok)
{
    if (!m_map || !m_map->has(name)) {
        if (ok)
            *ok = false;
        return 0.f;
    }
    const PlcPoint p = m_map->point(name);
    QByteArray buf;
    if (!readDb(p.db, p.offset, 4, &buf) || buf.size() < 4) {
        if (ok)
            *ok = false;
        return 0.f;
    }
    if (ok)
        *ok = true;
    return getBeFloat(buf, 0);
}

bool IPlcClient::writeReal(const QString &name, float value, QString *error)
{
    if (!m_map || !m_map->has(name)) {
        if (error)
            *error = QStringLiteral("Unknown point %1").arg(name);
        return false;
    }
    const PlcPoint p = m_map->point(name);
    QByteArray buf(4, 0);
    setBeFloat(&buf, 0, value);
    return writeDb(p.db, p.offset, buf, error);
}

bool IPlcClient::loopbackTest(QString *error)
{
    if (!isConnected()) {
        if (error)
            *error = QStringLiteral("PLC not connected");
        return false;
    }
    const quint16 magic = 0xA55A;
    if (!writeUInt16(QStringLiteral("FailCode"), magic, error))
        return false;
    bool ok = false;
    const quint16 back = readUInt16(QStringLiteral("FailCode"), &ok);
    if (!ok || back != magic) {
        if (error)
            *error = QStringLiteral("Loopback mismatch got %1").arg(back);
        return false;
    }
    writeUInt16(QStringLiteral("FailCode"), 0);
    return true;
}
