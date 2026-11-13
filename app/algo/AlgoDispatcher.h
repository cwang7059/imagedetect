#pragma once

#include "StationAlgoWorker.h"
#include "camera/FramePacket.h"
#include "core/ProductRecord.h"

#include <QObject>
#include <QThread>

/**
 * Routes frames to three station workers, each on its own QThread.
 * PLC/UI thread stays free; Python calls still serialize on the GIL/mutex.
 */
class AlgoDispatcher : public QObject
{
    Q_OBJECT
public:
    explicit AlgoDispatcher(QObject *parent = nullptr);
    ~AlgoDispatcher() override;

    bool prepare(const QString &projectRoot, const QString &calibDir, QString *error = nullptr);
    void shutdown();

    /** Async enqueue (QueuedConnection to worker thread). */
    void submitFrame(const FramePacket &frame);
    /** Block until that station finishes (dry-run / self-check). */
    void submitFrameBlocking(const FramePacket &frame);

signals:
    void odReady(quint16 slot, quint32 productSeq, const StationOdResult &r);
    void lengthReady(quint16 slot, quint32 productSeq, const StationLengthResult &r);
    void defectReady(quint16 slot, quint32 productSeq, const StationDefectResult &r);
    void algoFailed(quint16 slot, StationId station, const QString &err);
    void previewFrame(const FramePacket &frame);
    void statusMessage(const QString &msg);

public slots:
    void processFrame(const FramePacket &frame) { submitFrame(frame); }

private:
    StationAlgoWorker *workerFor(StationId id) const;
    void wireWorker(StationAlgoWorker *w);

    StationAlgoWorker *m_od = nullptr;
    StationAlgoWorker *m_len = nullptr;
    StationAlgoWorker *m_def = nullptr;
    QThread m_thOd;
    QThread m_thLen;
    QThread m_thDef;
    bool m_ready = false;
};

Q_DECLARE_METATYPE(StationOdResult)
Q_DECLARE_METATYPE(StationLengthResult)
Q_DECLARE_METATYPE(StationDefectResult)
