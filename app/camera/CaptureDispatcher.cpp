#include "CaptureDispatcher.h"

CaptureDispatcher::CaptureDispatcher(QObject *parent)
    : QObject(parent)
{
}

void CaptureDispatcher::registerCamera(ICamera *cam)
{
    if (!cam)
        return;
    m_cams.insert(static_cast<int>(cam->station()), cam);
    connect(cam, &ICamera::frameReady, this, &CaptureDispatcher::frameReady);
    connect(cam, &ICamera::cameraError, this, &CaptureDispatcher::failed);
}

void CaptureDispatcher::clear()
{
    m_cams.clear();
}

bool CaptureDispatcher::openAll(QString *error)
{
    for (auto it = m_cams.begin(); it != m_cams.end(); ++it) {
        QString err;
        if (!it.value()->openCamera(&err)) {
            if (error)
                *error = err;
            emit failed(err);
            return false;
        }
    }
    emit statusMessage(QStringLiteral("All cameras open (%1)").arg(m_cams.size()));
    return true;
}

void CaptureDispatcher::closeAll()
{
    for (auto it = m_cams.begin(); it != m_cams.end(); ++it)
        it.value()->closeCamera();
}

bool CaptureDispatcher::triggerStation(StationId station, quint16 slot, quint32 productSeq)
{
    ICamera *cam = m_cams.value(static_cast<int>(station), nullptr);
    if (!cam) {
        emit failed(QStringLiteral("No camera for station %1").arg(stationName(station)));
        return false;
    }
    QString err;
    if (!cam->onHardTrigger(slot, productSeq, &err)) {
        emit failed(err);
        return false;
    }
    return true;
}
