#pragma once

#include "IPlcClient.h"

#include <QTcpSocket>

/**
 * Modbus TCP client mapping DB byte offsets onto holding registers.
 * Register address = offset/2 (big-endian 16-bit words), unit id = db (low 8 bits).
 */
class ModbusTcpClient : public IPlcClient
{
    Q_OBJECT
public:
    explicit ModbusTcpClient(QObject *parent = nullptr);
    ~ModbusTcpClient() override;

    bool connectToPlc(const PlcConnectionConfig &cfg, QString *error = nullptr) override;
    void disconnectFromPlc() override;
    bool isConnected() const override;

    bool readDb(int db, int offset, int size, QByteArray *out, QString *error = nullptr) override;
    bool writeDb(int db, int offset, const QByteArray &data, QString *error = nullptr) override;

private:
    bool transact(quint8 unitId, const QByteArray &pdu, QByteArray *responsePdu, QString *error);
    quint16 nextTxId();

    QTcpSocket *m_sock = nullptr;
    quint16 m_txId = 1;
    int m_port = 502;
    QByteArray m_localMirror; // for bit RMW when reading-modify-writing bools
    int m_mirrorSize = 64;
};
