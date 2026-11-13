#include "MockPlcClient.h"

#include <cstring>

#include <QMutexLocker>
#include <QtEndian>

MockPlcClient::MockPlcClient(QObject *parent)
    : IPlcClient(parent)
{
}

bool MockPlcClient::connectToPlc(const PlcConnectionConfig &cfg, QString *error)
{
    Q_UNUSED(error);
    QMutexLocker lock(&m_mutex);
    m_dbSize = qMax(64, cfg.dbSizeBytes);
    m_dbs.clear();
    m_dbs.insert(cfg.dbNumber, QByteArray(m_dbSize, 0));
    // SysReady = true
    QByteArray &db = m_dbs[cfg.dbNumber];
    db[0] = static_cast<char>(db[0] | (1 << 1));
    m_connected = true;
    emit connectedChanged(true);
    return true;
}

void MockPlcClient::disconnectFromPlc()
{
    QMutexLocker lock(&m_mutex);
    m_connected = false;
    emit connectedChanged(false);
}

bool MockPlcClient::isConnected() const
{
    QMutexLocker lock(&m_mutex);
    return m_connected;
}

QByteArray &MockPlcClient::dbRef(int db)
{
    if (!m_dbs.contains(db))
        m_dbs.insert(db, QByteArray(m_dbSize, 0));
    return m_dbs[db];
}

bool MockPlcClient::readDb(int db, int offset, int size, QByteArray *out, QString *error)
{
    QMutexLocker lock(&m_mutex);
    if (!m_connected) {
        if (error)
            *error = QStringLiteral("Mock PLC disconnected");
        return false;
    }
    QByteArray &mem = dbRef(db);
    if (offset < 0 || size < 0 || offset + size > mem.size()) {
        if (error)
            *error = QStringLiteral("Mock read OOB");
        return false;
    }
    if (out)
        *out = mem.mid(offset, size);
    return true;
}

bool MockPlcClient::writeDb(int db, int offset, const QByteArray &data, QString *error)
{
    QMutexLocker lock(&m_mutex);
    if (!m_connected) {
        if (error)
            *error = QStringLiteral("Mock PLC disconnected");
        return false;
    }
    QByteArray &mem = dbRef(db);
    if (offset < 0 || offset + data.size() > mem.size()) {
        if (error)
            *error = QStringLiteral("Mock write OOB");
        return false;
    }
    memcpy(mem.data() + offset, data.constData(), static_cast<size_t>(data.size()));
    return true;
}

void MockPlcClient::simSetBool(int db, int offset, int bit, bool value)
{
    QMutexLocker lock(&m_mutex);
    QByteArray &mem = dbRef(db);
    if (offset < 0 || offset >= mem.size())
        return;
    uchar b = static_cast<uchar>(mem[offset]);
    if (value)
        b = static_cast<uchar>(b | (1 << bit));
    else
        b = static_cast<uchar>(b & ~(1 << bit));
    mem[offset] = static_cast<char>(b);
}

void MockPlcClient::simSetUInt16(int db, int offset, quint16 value)
{
    QMutexLocker lock(&m_mutex);
    QByteArray &mem = dbRef(db);
    if (offset + 2 > mem.size())
        return;
    qToBigEndian(value, reinterpret_cast<uchar *>(mem.data() + offset));
}

void MockPlcClient::simPulseTrig(int station, quint16 slot)
{
    // Trig bits at offset 1: bit0=ST1, bit1=ST2, bit2=ST3, bit3=Judge
    // Slots at 2,4,6,8
    const int bit = qBound(0, station - 1, 3);
    const int slotOff = 2 + (station - 1) * 2;
    simSetUInt16(100, slotOff, slot);
    simSetBool(100, 1, bit, true);
}
