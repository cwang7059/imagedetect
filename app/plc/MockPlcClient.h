#pragma once

#include "IPlcClient.h"

#include <QHash>
#include <QMutex>

/** In-memory DB for offline development and loopback without Snap7. */
class MockPlcClient : public IPlcClient
{
    Q_OBJECT
public:
    explicit MockPlcClient(QObject *parent = nullptr);

    bool connectToPlc(const PlcConnectionConfig &cfg, QString *error = nullptr) override;
    void disconnectFromPlc() override;
    bool isConnected() const override;

    bool readDb(int db, int offset, int size, QByteArray *out, QString *error = nullptr) override;
    bool writeDb(int db, int offset, const QByteArray &data, QString *error = nullptr) override;

    /** Simulation helpers for line dry-run */
    void simSetBool(int db, int offset, int bit, bool value);
    void simSetUInt16(int db, int offset, quint16 value);
    void simPulseTrig(int station /*1..4*/, quint16 slot);

private:
    QByteArray &dbRef(int db);

    mutable QMutex m_mutex;
    bool m_connected = false;
    int m_dbSize = 64;
    QHash<int, QByteArray> m_dbs;
};
