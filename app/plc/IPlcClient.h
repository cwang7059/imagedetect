#pragma once

#include "PlcPointMap.h"

#include <QByteArray>
#include <QObject>
#include <QString>

/**
 * Abstract PLC client. Business code uses named points from PlcPointMap.
 */
class IPlcClient : public QObject
{
    Q_OBJECT
public:
    explicit IPlcClient(QObject *parent = nullptr) : QObject(parent) {}
    ~IPlcClient() override = default;

    virtual bool connectToPlc(const PlcConnectionConfig &cfg, QString *error = nullptr) = 0;
    virtual void disconnectFromPlc() = 0;
    virtual bool isConnected() const = 0;

    virtual bool readDb(int db, int offset, int size, QByteArray *out, QString *error = nullptr) = 0;
    virtual bool writeDb(int db, int offset, const QByteArray &data, QString *error = nullptr) = 0;

    /** Named accessors using point map */
    void setPointMap(const PlcPointMap *map) { m_map = map; }
    const PlcPointMap *pointMap() const { return m_map; }

    bool readBool(const QString &name, bool *ok = nullptr);
    bool writeBool(const QString &name, bool value, QString *error = nullptr);
    quint16 readUInt16(const QString &name, bool *ok = nullptr);
    bool writeUInt16(const QString &name, quint16 value, QString *error = nullptr);
    quint32 readUInt32(const QString &name, bool *ok = nullptr);
    float readReal(const QString &name, bool *ok = nullptr);
    bool writeReal(const QString &name, float value, QString *error = nullptr);

    /** Round-trip test: write then read a PC→PLC scratch pattern */
    bool loopbackTest(QString *error = nullptr);

signals:
    void connectedChanged(bool on);
    void communicationError(const QString &msg);

protected:
    const PlcPointMap *m_map = nullptr;
};
