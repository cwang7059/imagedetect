#pragma once

#include "IPlcClient.h"

/**
 * Siemens S7 client.
 * Runtime-loads snap7.dll via Snap7Loader; falls back to MockPlcClient if DLL missing.
 */
class S7Client : public IPlcClient
{
    Q_OBJECT
public:
    explicit S7Client(QObject *parent = nullptr);
    ~S7Client() override;

    bool connectToPlc(const PlcConnectionConfig &cfg, QString *error = nullptr) override;
    void disconnectFromPlc() override;
    bool isConnected() const override;

    bool readDb(int db, int offset, int size, QByteArray *out, QString *error = nullptr) override;
    bool writeDb(int db, int offset, const QByteArray &data, QString *error = nullptr) override;

    bool usingRealSnap7() const;

private:
    class Impl;
    Impl *m_impl = nullptr;
};
