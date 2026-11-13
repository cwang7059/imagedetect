#pragma once

#include "../core/StationTypes.h"

#include <QByteArray>
#include <QMetaType>
#include <QString>
#include <QtGlobal>

struct FramePacket {
    StationId station = StationId::None;
    quint16 slot = 0;
    quint32 productSeq = 0;
    qint64 timestampMs = 0;
    int width = 0;
    int height = 0;
    int channels = 3; // BGR8
    QByteArray buffer;
    QString cameraId;
};

Q_DECLARE_METATYPE(FramePacket)
