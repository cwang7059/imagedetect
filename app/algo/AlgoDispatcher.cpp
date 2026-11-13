#include "AlgoDispatcher.h"

#include <QMetaType>

AlgoDispatcher::AlgoDispatcher(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<FramePacket>("FramePacket");
    qRegisterMetaType<StationOdResult>("StationOdResult");
    qRegisterMetaType<StationLengthResult>("StationLengthResult");
    qRegisterMetaType<StationDefectResult>("StationDefectResult");

    m_od = new StationAlgoWorker(StationId::ST1_Od);
    m_len = new StationAlgoWorker(StationId::ST2_Length);
    m_def = new StationAlgoWorker(StationId::ST3_Defect);
    m_od->setMaxQueue(2);
    m_len->setMaxQueue(2);
    m_def->setMaxQueue(2);

    m_od->moveToThread(&m_thOd);
    m_len->moveToThread(&m_thLen);
    m_def->moveToThread(&m_thDef);

    connect(&m_thOd, &QThread::finished, m_od, &QObject::deleteLater);
    connect(&m_thLen, &QThread::finished, m_len, &QObject::deleteLater);
    connect(&m_thDef, &QThread::finished, m_def, &QObject::deleteLater);

    wireWorker(m_od);
    wireWorker(m_len);
    wireWorker(m_def);
}

AlgoDispatcher::~AlgoDispatcher()
{
    shutdown();
}

void AlgoDispatcher::wireWorker(StationAlgoWorker *w)
{
    connect(w, &StationAlgoWorker::odReady, this, &AlgoDispatcher::odReady, Qt::QueuedConnection);
    connect(w, &StationAlgoWorker::lengthReady, this, &AlgoDispatcher::lengthReady, Qt::QueuedConnection);
    connect(w, &StationAlgoWorker::defectReady, this, &AlgoDispatcher::defectReady, Qt::QueuedConnection);
    connect(w, &StationAlgoWorker::algoFailed, this, &AlgoDispatcher::algoFailed, Qt::QueuedConnection);
    connect(w, &StationAlgoWorker::previewFrame, this, &AlgoDispatcher::previewFrame, Qt::QueuedConnection);
}

StationAlgoWorker *AlgoDispatcher::workerFor(StationId id) const
{
    switch (id) {
    case StationId::ST1_Od: return m_od;
    case StationId::ST2_Length: return m_len;
    case StationId::ST3_Defect: return m_def;
    default: return nullptr;
    }
}

bool AlgoDispatcher::prepare(const QString &projectRoot, const QString &calibDir, QString *error)
{
    if (m_ready)
        return true;

    m_thOd.start();
    m_thLen.start();
    m_thDef.start();

    // Prepare each worker on its own thread (Python init under global mutex).
    auto prep = [&](StationAlgoWorker *w, QThread *th) -> bool {
        bool ok = false;
        QString err;
        QMetaObject::invokeMethod(
            w,
            [w, projectRoot, calibDir, &ok, &err]() {
                ok = w->prepare(projectRoot, calibDir, &err);
            },
            Qt::BlockingQueuedConnection);
        if (!ok) {
            if (error)
                *error = err.isEmpty() ? QStringLiteral("Algo worker prepare failed") : err;
            return false;
        }
        Q_UNUSED(th);
        return true;
    };

    if (!prep(m_od, &m_thOd) || !prep(m_len, &m_thLen) || !prep(m_def, &m_thDef)) {
        shutdown();
        return false;
    }

    m_ready = true;
    emit statusMessage(QStringLiteral("Algo pipeline: 3 station threads ready"));
    return true;
}

void AlgoDispatcher::shutdown()
{
    m_ready = false;
    auto stop = [](QThread &th) {
        if (th.isRunning()) {
            th.quit();
            th.wait(3000);
        }
    };
    stop(m_thOd);
    stop(m_thLen);
    stop(m_thDef);
    m_od = m_len = m_def = nullptr;
}

void AlgoDispatcher::submitFrame(const FramePacket &frame)
{
    if (!m_ready)
        return;
    StationAlgoWorker *w = workerFor(frame.station);
    if (!w)
        return;
    QMetaObject::invokeMethod(
        w,
        "processFrame",
        Qt::QueuedConnection,
        Q_ARG(FramePacket, frame));
}

void AlgoDispatcher::submitFrameBlocking(const FramePacket &frame)
{
    if (!m_ready)
        return;
    StationAlgoWorker *w = workerFor(frame.station);
    if (!w)
        return;
    QMetaObject::invokeMethod(
        w,
        "processFrame",
        Qt::BlockingQueuedConnection,
        Q_ARG(FramePacket, frame));
}
