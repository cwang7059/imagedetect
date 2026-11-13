#pragma once

#include "PythonBridge.h"
#include "camera/FramePacket.h"
#include "core/ProductRecord.h"

#include <QAtomicInt>
#include <QObject>

/**
 * One PythonBridge + one station, meant to live on its own QThread.
 * Bounded in-flight: if busy beyond maxQueue, frame is dropped as AlgoError.
 */
class StationAlgoWorker : public QObject
{
    Q_OBJECT
public:
    explicit StationAlgoWorker(StationId station, QObject *parent = nullptr);

    StationId station() const { return m_station; }
    void setMaxQueue(int n);

public slots:
    bool prepare(const QString &projectRoot, const QString &calibDir, QString *error = nullptr);
    void processFrame(const FramePacket &frame);

signals:
    void odReady(quint16 slot, quint32 productSeq, const StationOdResult &r);
    void lengthReady(quint16 slot, quint32 productSeq, const StationLengthResult &r);
    void defectReady(quint16 slot, quint32 productSeq, const StationDefectResult &r);
    void algoFailed(quint16 slot, StationId station, const QString &err);
    void previewFrame(const FramePacket &frame);

private:
    void emitDropAsError(const FramePacket &frame, const QString &why);

    StationId m_station = StationId::None;
    PythonBridge m_bridge;
    bool m_ready = false;
    QAtomicInt m_inflight { 0 };
    int m_maxQueue = 2;
};
