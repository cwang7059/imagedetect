#pragma once

#include "FramePacket.h"

#include <QObject>
#include <QString>

class ICamera : public QObject
{
    Q_OBJECT
public:
    explicit ICamera(QObject *parent = nullptr) : QObject(parent) {}
    ~ICamera() override = default;

    virtual QString cameraId() const = 0;
    virtual StationId station() const = 0;

    virtual bool openCamera(QString *error = nullptr) = 0;
    virtual void closeCamera() = 0;
    virtual bool isOpen() const = 0;

    /**
     * Soft notify that PLC hard-triggered; some SDKs need this to pull the frame.
     * Implementations may also generate a synthetic frame in mock mode.
     */
    virtual bool onHardTrigger(quint16 slot, quint32 productSeq, QString *error = nullptr) = 0;

signals:
    void frameReady(const FramePacket &frame);
    void cameraError(const QString &msg);
    void onlineChanged(bool online);
};
