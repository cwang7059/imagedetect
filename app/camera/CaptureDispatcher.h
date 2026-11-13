#pragma once

#include "ICamera.h"

#include <QHash>
#include <QObject>

/**
 * Routes PLC soft-triggers to the correct camera and re-emits FramePackets.
 */
class CaptureDispatcher : public QObject
{
    Q_OBJECT
public:
    explicit CaptureDispatcher(QObject *parent = nullptr);

    void registerCamera(ICamera *cam);
    void clear();

    bool openAll(QString *error = nullptr);
    void closeAll();

public slots:
    bool triggerStation(StationId station, quint16 slot, quint32 productSeq);

signals:
    void frameReady(const FramePacket &frame);
    void statusMessage(const QString &msg);
    void failed(const QString &err);

private:
    QHash<int, ICamera *> m_cams; // StationId -> camera
};
