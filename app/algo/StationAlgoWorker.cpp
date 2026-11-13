#include "StationAlgoWorker.h"

#include <QElapsedTimer>

StationAlgoWorker::StationAlgoWorker(StationId station, QObject *parent)
    : QObject(parent)
    , m_station(station)
{
}

void StationAlgoWorker::setMaxQueue(int n)
{
    m_maxQueue = qMax(1, n);
}

bool StationAlgoWorker::prepare(const QString &projectRoot, const QString &calibDir, QString *error)
{
    if (!m_bridge.initialize(projectRoot, error))
        return false;
    if (!m_bridge.initPipeline(180, 80, error))
        return false;
    if (!calibDir.isEmpty()) {
        if (!m_bridge.loadCalibDir(calibDir, error))
            return false;
    }
    m_ready = true;
    return true;
}

void StationAlgoWorker::emitDropAsError(const FramePacket &frame, const QString &why)
{
    emit algoFailed(frame.slot, m_station, why);
    switch (m_station) {
    case StationId::ST1_Od: {
        StationOdResult r;
        r.ready = true;
        r.algoOk = false;
        emit odReady(frame.slot, frame.productSeq, r);
        break;
    }
    case StationId::ST2_Length: {
        StationLengthResult r;
        r.ready = true;
        r.algoOk = false;
        emit lengthReady(frame.slot, frame.productSeq, r);
        break;
    }
    case StationId::ST3_Defect: {
        StationDefectResult r;
        r.ready = true;
        r.algoOk = false;
        r.summary = why;
        emit defectReady(frame.slot, frame.productSeq, r);
        break;
    }
    default:
        break;
    }
}

void StationAlgoWorker::processFrame(const FramePacket &frame)
{
    if (!m_ready || frame.buffer.isEmpty() || frame.station != m_station)
        return;

    if (m_inflight.load() >= m_maxQueue) {
        emitDropAsError(frame, QStringLiteral("Algo queue full — frame dropped"));
        return;
    }

    m_inflight.fetchAndAddOrdered(1);
    emit previewFrame(frame);

    const uchar *bgr = reinterpret_cast<const uchar *>(frame.buffer.constData());
    QElapsedTimer t;
    t.start();
    QString err;

    switch (m_station) {
    case StationId::ST1_Od: {
        OdMeasureResult m;
        StationOdResult r;
        r.ready = true;
        if (!m_bridge.measureOd(bgr, frame.height, frame.width, &m, &err)) {
            r.algoOk = false;
            r.elapsedMs = t.elapsed();
            emit algoFailed(frame.slot, m_station, err);
        } else {
            r.algoOk = m.ok;
            r.d1Mm = m.d1Mm;
            r.d2Mm = m.d2Mm;
            r.score = m.score;
            r.elapsedMs = t.elapsed();
        }
        emit odReady(frame.slot, frame.productSeq, r);
        break;
    }
    case StationId::ST2_Length: {
        LengthMeasureResult m;
        StationLengthResult r;
        r.ready = true;
        if (!m_bridge.measureLength(bgr, frame.height, frame.width, &m, &err)) {
            r.algoOk = false;
            r.elapsedMs = t.elapsed();
            emit algoFailed(frame.slot, m_station, err);
        } else {
            r.algoOk = m.ok;
            r.lengthMm = m.lengthMm;
            r.elapsedMs = t.elapsed();
        }
        emit lengthReady(frame.slot, frame.productSeq, r);
        break;
    }
    case StationId::ST3_Defect: {
        DefectMeasureResult m;
        StationDefectResult r;
        r.ready = true;
        if (!m_bridge.inspectDefect(bgr, frame.height, frame.width, &m, &err)) {
            r.algoOk = false;
            r.elapsedMs = t.elapsed();
            emit algoFailed(frame.slot, m_station, err);
        } else {
            r.algoOk = m.ok;
            r.grade = m.grade;
            r.defectCount = m.defectCount;
            r.summary = m.summary;
            r.elapsedMs = t.elapsed();
        }
        emit defectReady(frame.slot, frame.productSeq, r);
        break;
    }
    default:
        break;
    }

    m_inflight.fetchAndSubOrdered(1);
}
