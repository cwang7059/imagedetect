#include "ModbusTcpClient.h"

#include <cstring>

#include <QtEndian>

ModbusTcpClient::ModbusTcpClient(QObject *parent)
    : IPlcClient(parent)
    , m_sock(new QTcpSocket(this))
{
    m_localMirror = QByteArray(m_mirrorSize, 0);
}

ModbusTcpClient::~ModbusTcpClient()
{
    disconnectFromPlc();
}

quint16 ModbusTcpClient::nextTxId()
{
    return m_txId++;
}

bool ModbusTcpClient::connectToPlc(const PlcConnectionConfig &cfg, QString *error)
{
    m_port = 502;
    m_mirrorSize = qMax(64, cfg.dbSizeBytes);
    m_localMirror = QByteArray(m_mirrorSize, 0);

    m_sock->abort();
    m_sock->connectToHost(cfg.ip, static_cast<quint16>(m_port));
    if (!m_sock->waitForConnected(2000)) {
        if (error)
            *error = QStringLiteral("Modbus TCP connect failed: %1").arg(m_sock->errorString());
        emit connectedChanged(false);
        return false;
    }
    emit connectedChanged(true);
    return true;
}

void ModbusTcpClient::disconnectFromPlc()
{
    if (m_sock->state() != QAbstractSocket::UnconnectedState) {
        m_sock->disconnectFromHost();
        if (m_sock->state() != QAbstractSocket::UnconnectedState)
            m_sock->waitForDisconnected(500);
    }
    emit connectedChanged(false);
}

bool ModbusTcpClient::isConnected() const
{
    return m_sock && m_sock->state() == QAbstractSocket::ConnectedState;
}

bool ModbusTcpClient::transact(quint8 unitId, const QByteArray &pdu, QByteArray *responsePdu, QString *error)
{
    if (!isConnected()) {
        if (error)
            *error = QStringLiteral("Modbus not connected");
        return false;
    }
    QByteArray frame;
    frame.resize(7);
    qToBigEndian(nextTxId(), reinterpret_cast<uchar *>(frame.data() + 0));
    qToBigEndian(quint16(0), reinterpret_cast<uchar *>(frame.data() + 2)); // protocol
    qToBigEndian(quint16(1 + pdu.size()), reinterpret_cast<uchar *>(frame.data() + 4));
    frame.append(char(unitId));
    frame.append(pdu);

    if (m_sock->write(frame) != frame.size() || !m_sock->waitForBytesWritten(1000)) {
        if (error)
            *error = QStringLiteral("Modbus write failed");
        return false;
    }

    // MBAP header 7 bytes
    while (m_sock->bytesAvailable() < 7) {
        if (!m_sock->waitForReadyRead(1000)) {
            if (error)
                *error = QStringLiteral("Modbus response timeout");
            return false;
        }
    }
    QByteArray hdr = m_sock->read(7);
    const quint16 len = qFromBigEndian<quint16>(reinterpret_cast<const uchar *>(hdr.constData() + 4));
    const int need = int(len) - 1; // remaining after unit id already in hdr[6]
    while (m_sock->bytesAvailable() < need) {
        if (!m_sock->waitForReadyRead(1000)) {
            if (error)
                *error = QStringLiteral("Modbus PDU timeout");
            return false;
        }
    }
    QByteArray respPdu = m_sock->read(need);
    if (respPdu.isEmpty()) {
        if (error)
            *error = QStringLiteral("Modbus empty PDU");
        return false;
    }
    if (static_cast<quint8>(respPdu.at(0)) & 0x80) {
        if (error)
            *error = QStringLiteral("Modbus exception 0x%1")
                         .arg(respPdu.size() > 1 ? quint8(respPdu.at(1)) : 0, 2, 16, QLatin1Char('0'));
        return false;
    }
    if (responsePdu)
        *responsePdu = respPdu;
    return true;
}

bool ModbusTcpClient::readDb(int db, int offset, int size, QByteArray *out, QString *error)
{
    if (offset < 0 || size <= 0) {
        if (error)
            *error = QStringLiteral("Modbus bad range");
        return false;
    }
    // Pad to even register boundary
    const int startByte = offset & ~1;
    const int endByte = (offset + size + 1) & ~1;
    const int byteCount = endByte - startByte;
    const quint16 startReg = static_cast<quint16>(startByte / 2);
    const quint16 qty = static_cast<quint16>(byteCount / 2);
    const quint8 unit = static_cast<quint8>(db & 0xFF);

    QByteArray pdu;
    pdu.append(char(0x03)); // Read Holding Registers
    char be[4];
    qToBigEndian(startReg, reinterpret_cast<uchar *>(be));
    qToBigEndian(qty, reinterpret_cast<uchar *>(be + 2));
    pdu.append(be, 4);

    QByteArray resp;
    if (!transact(unit, pdu, &resp, error))
        return false;
    if (resp.size() < 2 || static_cast<quint8>(resp.at(0)) != 0x03) {
        if (error)
            *error = QStringLiteral("Modbus unexpected FC");
        return false;
    }
    const int bc = static_cast<quint8>(resp.at(1));
    if (resp.size() < 2 + bc || bc < byteCount) {
        if (error)
            *error = QStringLiteral("Modbus short data");
        return false;
    }
    QByteArray chunk = resp.mid(2, bc);
    // Refresh local mirror for RMW bools
    if (startByte + chunk.size() <= m_localMirror.size())
        memcpy(m_localMirror.data() + startByte, chunk.constData(), static_cast<size_t>(chunk.size()));

    if (out)
        *out = chunk.mid(offset - startByte, size);
    return true;
}

bool ModbusTcpClient::writeDb(int db, int offset, const QByteArray &data, QString *error)
{
    if (offset < 0 || data.isEmpty()) {
        if (error)
            *error = QStringLiteral("Modbus bad write");
        return false;
    }
    // Merge into mirror then write spanning registers
    if (offset + data.size() > m_localMirror.size()) {
        m_mirrorSize = offset + data.size() + 16;
        m_localMirror.resize(m_mirrorSize);
    }
    memcpy(m_localMirror.data() + offset, data.constData(), static_cast<size_t>(data.size()));

    const int startByte = offset & ~1;
    const int endByte = (offset + data.size() + 1) & ~1;
    const int byteCount = endByte - startByte;
    const quint16 startReg = static_cast<quint16>(startByte / 2);
    const quint16 qty = static_cast<quint16>(byteCount / 2);
    const quint8 unit = static_cast<quint8>(db & 0xFF);

    QByteArray pdu;
    pdu.append(char(0x10)); // Write Multiple Registers
    char be[5];
    qToBigEndian(startReg, reinterpret_cast<uchar *>(be));
    qToBigEndian(qty, reinterpret_cast<uchar *>(be + 2));
    be[4] = static_cast<char>(byteCount);
    pdu.append(be, 5);
    pdu.append(m_localMirror.mid(startByte, byteCount));

    QByteArray resp;
    if (!transact(unit, pdu, &resp, error))
        return false;
    return true;
}
