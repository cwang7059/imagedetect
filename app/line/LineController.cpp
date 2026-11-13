#include "LineController.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>

LineController::LineController(QObject *parent)
    : QObject(parent)
{
    m_plc = &m_s7;
    m_tracker.setJudge(&m_judge);

    connect(&m_pollTimer, &QTimer::timeout, this, &LineController::onPoll);
    connect(&m_hbTimer, &QTimer::timeout, this, &LineController::onHeartbeat);
    connect(&m_slotTimeoutTimer, &QTimer::timeout, this, &LineController::onSlotTimeoutScan);
    m_slotTimeoutTimer.setInterval(50);

    wireCaptureToAlgo();
    connect(&m_capture, &CaptureDispatcher::failed, this, &LineController::alarm);
    connect(&m_capture, &CaptureDispatcher::statusMessage, this, &LineController::statusMessage);
    connect(&m_algo, &AlgoDispatcher::statusMessage, this, &LineController::statusMessage);

    connect(&m_algo, &AlgoDispatcher::odReady, this, &LineController::onOd);
    connect(&m_algo, &AlgoDispatcher::lengthReady, this, &LineController::onLength);
    connect(&m_algo, &AlgoDispatcher::defectReady, this, &LineController::onDefect);
    connect(&m_algo, &AlgoDispatcher::algoFailed, this,
            [this](quint16 slot, StationId st, const QString &err) {
                emit alarm(QStringLiteral("Algo slot %1 %2: %3")
                               .arg(slot)
                               .arg(stationName(st))
                               .arg(err));
            });
    // Keep last frame cache for NG save; live UI already updated from capture.
    connect(&m_algo, &AlgoDispatcher::previewFrame, this, &LineController::onPreviewCache);

    connect(&m_livePreviewTimer, &QTimer::timeout, this, &LineController::onLivePreviewTick);
    m_livePreviewTimer.setInterval(200);

    connect(&m_tracker, &ProductTracker::judged, this, &LineController::onJudged);
    connect(&m_tracker, &ProductTracker::recordUpdated, this, &LineController::recordUpdated);

    connect(&m_s7, &IPlcClient::connectedChanged, this, [this](bool on) {
        emit plcStateChanged(on, m_s7.usingRealSnap7()
                                     ? QStringLiteral("S7 Snap7")
                                     : QStringLiteral("S7 mock fallback"));
    });
    connect(&m_s7, &IPlcClient::communicationError, this, &LineController::alarm);
}

LineController::~LineController()
{
    setLivePreviewEnabled(false);
    stopLine();
    disconnectPlc();
    m_capture.closeAll();
    m_algo.shutdown();
}

void LineController::wireCaptureToAlgo()
{
    connect(&m_capture, &CaptureDispatcher::frameReady, this, [this](const FramePacket &frame) {
        emit previewFrame(frame); // always push to station camera windows
        if (m_previewOnly)
            return;
        armSlotDeadline(frame.slot);
        m_algo.submitFrame(frame);
    });
}

void LineController::setLivePreviewEnabled(bool on)
{
    m_livePreview = on;
    if (on)
        m_livePreviewTimer.start();
    else
        m_livePreviewTimer.stop();
    emit statusMessage(on ? QStringLiteral("工位相机实时预览已开启")
                          : QStringLiteral("工位相机实时预览已关闭"));
}

void LineController::refreshLivePreview()
{
    onLivePreviewTick();
}

void LineController::onLivePreviewTick()
{
    if (!m_camOd || !m_camLen || !m_camDef)
        return;
    m_previewOnly = true;
    m_capture.triggerStation(StationId::ST1_Od, 0, 0);
    m_capture.triggerStation(StationId::ST2_Length, 0, 0);
    m_capture.triggerStation(StationId::ST3_Defect, 0, 0);
    m_previewOnly = false;
}

bool LineController::initialize(const QString &projectRoot, QString *error)
{
    m_projectRoot = projectRoot;
    const QString pointsPath = QDir(projectRoot).filePath(QStringLiteral("config/plc_points.json"));
    if (!m_points.loadFromFile(pointsPath, error))
        return false;
    m_s7.setPointMap(&m_points);

    const QString recipePath = QDir(projectRoot).filePath(QStringLiteral("config/recipes/default.json"));
    m_recipe = Recipe::loadFromFile(recipePath, error);
    m_judge.setRecipe(m_recipe);

    // Algo Python workers are prepared lazily in startLine() so the UI can open quickly.
    m_calibDir = QDir(projectRoot).filePath(QStringLiteral("config/calib"));

    m_camOd = new HikLineScanCamera(StationId::ST1_Od, QStringLiteral("hik_od"), this);
    m_camLen = new HikLineScanCamera(StationId::ST2_Length, QStringLiteral("hik_len"), this);
    m_camDef = new OnnaLineScanCamera(QStringLiteral("onna_defect"), this);

    // Default: simulation only. MVS GigE EnumDevices can hang the UI for minutes.
    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const bool forceSim = env.contains(QStringLiteral("IMAGEDETECT_FORCE_SIM"))
        && env.value(QStringLiteral("IMAGEDETECT_FORCE_SIM")) != QStringLiteral("0");
    bool preferSim = true;
    {
        QFile cf(QDir(projectRoot).filePath(QStringLiteral("config/cameras.json")));
        if (cf.open(QIODevice::ReadOnly)) {
            const QJsonObject root = QJsonDocument::fromJson(cf.readAll()).object();
            if (root.contains(QStringLiteral("prefer_sim")))
                preferSim = root.value(QStringLiteral("prefer_sim")).toBool(true);
            auto applyHik = [forceSim, preferSim](HikLineScanCamera *cam, const QJsonObject &o) {
                if (!cam)
                    return;
                if (o.contains(QStringLiteral("index")))
                    cam->setDeviceIndex(static_cast<unsigned int>(o.value(QStringLiteral("index")).toInt(0)));
                const QString sn = o.value(QStringLiteral("serial")).toString();
                if (!sn.isEmpty())
                    cam->setSerialNumber(sn);
                const bool useReal = !forceSim && !preferSim
                    && o.value(QStringLiteral("use_real")).toBool(false);
                cam->setUseReal(useReal);
            };
            applyHik(m_camOd, root.value(QStringLiteral("hik_od")).toObject());
            applyHik(m_camLen, root.value(QStringLiteral("hik_len")).toObject());
            const QJsonObject onna = root.value(QStringLiteral("onna_defect")).toObject();
            if (!onna.isEmpty()) {
                m_camDef->setDeviceIndex(onna.value(QStringLiteral("index")).toInt(0));
                const QString sn = onna.value(QStringLiteral("serial")).toString();
                if (!sn.isEmpty())
                    m_camDef->setSerialNumber(sn);
            }
            const bool onnaReal = !forceSim && !preferSim
                && onna.value(QStringLiteral("use_real")).toBool(false);
            m_camDef->setUseReal(onnaReal);
        } else if (forceSim || preferSim) {
            m_camOd->setUseReal(false);
            m_camLen->setUseReal(false);
            m_camDef->setUseReal(false);
        }
    }

    m_capture.registerCamera(m_camOd);
    m_capture.registerCamera(m_camLen);
    m_capture.registerCamera(m_camDef);

    QString camErr;
    if (!m_capture.openAll(&camErr)) {
        // Soft-fail: keep app usable with remaining sim cameras
        emit statusMessage(QStringLiteral("Camera open warning: %1").arg(camErr));
        // Try open individually so sim fallbacks still work
        m_camOd->openCamera();
        m_camLen->openCamera();
        m_camDef->openCamera();
    }

    emit statusMessage(QStringLiteral("Cameras: hik_od=%1 hik_len=%2 onna=%3")
                           .arg(m_camOd->usingRealSdk() ? QStringLiteral("MVS") : QStringLiteral("sim"))
                           .arg(m_camLen->usingRealSdk() ? QStringLiteral("MVS") : QStringLiteral("sim"))
                           .arg(m_camDef->usingRealSdk() ? QStringLiteral("DLL") : QStringLiteral("sim")));

    const QString logDir = QDir(projectRoot).filePath(QStringLiteral("logs"));
    QDir().mkpath(logDir);
    if (!m_trace.openLog(QDir(logDir).filePath(QStringLiteral("results.csv")), error))
        return false;
    m_trace.setImageDir(QDir(logDir).filePath(QStringLiteral("ng_images")));

    emit statusMessage(QStringLiteral("LineController ready (recipe=%1, timeout=%2ms)")
                           .arg(m_recipe.name)
                           .arg(m_recipe.resultTimeoutMs));
    return true;
}

bool LineController::setRecipePath(const QString &path, QString *error)
{
    m_recipe = Recipe::loadFromFile(path, error);
    m_judge.setRecipe(m_recipe);
    return true;
}

bool LineController::plcConnected() const
{
    return m_plc && m_plc->isConnected();
}

bool LineController::usingMockPlc() const
{
    return !m_s7.usingRealSnap7();
}

bool LineController::connectPlc(QString *error)
{
    PlcConnectionConfig cfg = m_points.connection();
    if (!m_s7.usingRealSnap7())
        cfg.protocol = QStringLiteral("mock");
    if (!m_s7.connectToPlc(cfg, error))
        return false;
    m_plc = &m_s7;
    emit statusMessage(QStringLiteral("PLC connected (%1)").arg(cfg.ip));
    return true;
}

void LineController::disconnectPlc()
{
    if (m_plc)
        m_plc->disconnectFromPlc();
}

bool LineController::runLoopbackTest(QString *error)
{
    if (!m_plc || !m_plc->isConnected()) {
        if (error)
            *error = QStringLiteral("PLC not connected");
        return false;
    }
    return m_plc->loopbackTest(error);
}

bool LineController::runSelfCheck(QString *error)
{
    if (!connectPlc(error))
        return false;
    if (!runLoopbackTest(error))
        return false;
    simulateOneCycle(0);
    emit statusMessage(QStringLiteral("Self-check OK (PLC loopback + blocking algo dry-run)"));
    return true;
}

void LineController::startLine()
{
    if (m_running)
        return;
    {
        QString err;
        if (!m_algo.prepare(m_projectRoot, m_calibDir, &err)) {
            emit alarm(QStringLiteral("Algo prepare failed: %1").arg(err));
            return;
        }
    }
    if (!m_plc || !m_plc->isConnected()) {
        QString err;
        if (!connectPlc(&err)) {
            emit alarm(err);
            return;
        }
    }
    m_running = true;
    m_pollTimer.start(qMax(10, m_points.connection().pollMs));
    m_hbTimer.start(qMax(100, m_points.connection().heartbeatMs));
    m_slotTimeoutTimer.start();
    emit lineRunningChanged(true);
    emit statusMessage(QStringLiteral("Line running (algo threads + slot timeout %1ms)")
                           .arg(m_recipe.resultTimeoutMs));
}

void LineController::stopLine()
{
    m_pollTimer.stop();
    m_hbTimer.stop();
    m_slotTimeoutTimer.stop();
    if (m_running) {
        m_running = false;
        emit lineRunningChanged(false);
        emit statusMessage(QStringLiteral("Line stopped"));
    }
    if (m_plc && m_plc->isConnected()) {
        m_plc->writeBool(QStringLiteral("Busy"), false);
        m_plc->writeBool(QStringLiteral("PcAlive"), false);
    }
}

void LineController::onHeartbeat()
{
    if (!m_plc || !m_plc->isConnected())
        return;
    m_pcAlive = !m_pcAlive;
    m_plc->writeBool(QStringLiteral("PcAlive"), m_pcAlive);
}

void LineController::armSlotDeadline(quint16 slot)
{
    if (m_slotDeadlineMs.contains(slot))
        return;
    const int ms = qMax(50, m_recipe.resultTimeoutMs);
    m_slotDeadlineMs.insert(slot, QDateTime::currentMSecsSinceEpoch() + ms);
}

void LineController::clearSlotDeadline(quint16 slot)
{
    m_slotDeadlineMs.remove(slot);
}

void LineController::onSlotTimeoutScan()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const QList<quint16> slotKeys = m_slotDeadlineMs.keys();
    for (int i = 0; i < slotKeys.size(); ++i) {
        const quint16 slot = slotKeys.at(i);
        if (now < m_slotDeadlineMs.value(slot))
            continue;
        ProductRecord rec;
        if (m_tracker.get(slot, &rec) && rec.judged) {
            clearSlotDeadline(slot);
            continue;
        }
        if (m_tracker.forceTimeoutJudge(slot, &rec)) {
            emit alarm(QStringLiteral("Slot %1 timed out (%2ms) — forced NG")
                           .arg(slot)
                           .arg(m_recipe.resultTimeoutMs));
        } else {
            // Slot never created but deadline armed — create then timeout
            m_tracker.upsertSlot(slot);
            if (m_tracker.forceTimeoutJudge(slot, &rec)) {
                emit alarm(QStringLiteral("Slot %1 timed out with no results — forced NG").arg(slot));
            }
        }
        clearSlotDeadline(slot);
    }
}

bool LineController::edgeTrig(const QString &trigName, bool *rising)
{
    bool ok = false;
    const bool now = m_plc->readBool(trigName, &ok);
    if (!ok)
        return false;
    int idx = 0;
    if (trigName.endsWith(QStringLiteral("ST2")))
        idx = 1;
    else if (trigName.endsWith(QStringLiteral("ST3")))
        idx = 2;
    else if (trigName.contains(QStringLiteral("Judge")))
        idx = 3;
    const bool rise = now && !m_prevTrig[idx];
    m_prevTrig[idx] = now;
    if (rising)
        *rising = rise;
    return rise;
}

void LineController::clearTrigEdge(const QString &trigName)
{
    if (usingMockPlc())
        m_plc->writeBool(trigName, false);
}

void LineController::onPoll()
{
    if (!m_plc || !m_plc->isConnected())
        return;

    m_plc->writeBool(QStringLiteral("Busy"), true);

    bool rise = false;
    if (edgeTrig(QStringLiteral("Trig_ST1"), &rise) && rise) {
        const quint16 slot = m_plc->readUInt16(QStringLiteral("Slot_ST1"));
        const quint32 seq = m_plc->readUInt32(QStringLiteral("ProductSeq"));
        armSlotDeadline(slot);
        m_capture.triggerStation(StationId::ST1_Od, slot, seq);
        clearTrigEdge(QStringLiteral("Trig_ST1"));
    }
    if (edgeTrig(QStringLiteral("Trig_ST2"), &rise) && rise) {
        const quint16 slot = m_plc->readUInt16(QStringLiteral("Slot_ST2"));
        const quint32 seq = m_plc->readUInt32(QStringLiteral("ProductSeq"));
        armSlotDeadline(slot);
        m_capture.triggerStation(StationId::ST2_Length, slot, seq);
        clearTrigEdge(QStringLiteral("Trig_ST2"));
    }
    if (edgeTrig(QStringLiteral("Trig_ST3"), &rise) && rise) {
        const quint16 slot = m_plc->readUInt16(QStringLiteral("Slot_ST3"));
        const quint32 seq = m_plc->readUInt32(QStringLiteral("ProductSeq"));
        armSlotDeadline(slot);
        m_capture.triggerStation(StationId::ST3_Defect, slot, seq);
        clearTrigEdge(QStringLiteral("Trig_ST3"));
    }
    if (edgeTrig(QStringLiteral("Trig_Judge"), &rise) && rise) {
        const quint16 slot = m_plc->readUInt16(QStringLiteral("Slot_Judge"));
        ProductRecord rec;
        if (!m_tracker.tryJudge(slot, &rec))
            m_tracker.forceTimeoutJudge(slot, &rec);
        clearSlotDeadline(slot);
        clearTrigEdge(QStringLiteral("Trig_Judge"));
    }

    bool ok = false;
    if (m_plc->readBool(QStringLiteral("Reset"), &ok) && ok) {
        m_tracker.clear();
        m_slotDeadlineMs.clear();
        emit statsChanged(0, 0);
        m_plc->writeBool(QStringLiteral("Reset"), false);
        emit statusMessage(QStringLiteral("Tracker reset by PLC"));
    }
}

void LineController::onOd(quint16 slot, quint32 seq, const StationOdResult &r)
{
    m_tracker.setOd(slot, r, seq);
    m_tracker.tryJudge(slot);
}

void LineController::onLength(quint16 slot, quint32 seq, const StationLengthResult &r)
{
    m_tracker.setLength(slot, r, seq);
    m_tracker.tryJudge(slot);
}

void LineController::onDefect(quint16 slot, quint32 seq, const StationDefectResult &r)
{
    m_tracker.setDefect(slot, r, seq);
    m_tracker.tryJudge(slot);
}

quint32 LineController::frameKey(quint16 slot, StationId st)
{
    return (static_cast<quint32>(slot) << 8) | static_cast<quint32>(st);
}

void LineController::onPreviewCache(const FramePacket &frame)
{
    m_lastFrames.insert(frameKey(frame.slot, frame.station), frame);
}

QString LineController::maybeSaveNgImage(ProductRecord *rec)
{
    if (!rec || rec->pass || !m_recipe.saveNgImage)
        return QString();
    const StationId order[] = {
        StationId::ST3_Defect, StationId::ST1_Od, StationId::ST2_Length
    };
    for (StationId st : order) {
        const quint32 key = frameKey(rec->slot, st);
        if (!m_lastFrames.contains(key))
            continue;
        const FramePacket &fp = m_lastFrames[key];
        const QString path = m_trace.saveNgImage(*rec, fp.buffer, fp.width, fp.height, fp.channels);
        if (!path.isEmpty()) {
            rec->ngImagePath = path;
            emit statusMessage(QStringLiteral("NG image saved: %1").arg(path));
            return path;
        }
    }
    return QString();
}

void LineController::onJudged(const ProductRecord &rec)
{
    clearSlotDeadline(rec.slot);
    ProductRecord out = rec;
    out.recipeId = m_recipe.id;
    maybeSaveNgImage(&out);
    writeRejectToPlc(out);
    m_trace.append(out);
    emit recordJudged(out);
    emit statsChanged(m_tracker.okCount(), m_tracker.ngCount());
    emit statusMessage(QStringLiteral("Judged slot %1 %2 (%3)")
                           .arg(out.slot)
                           .arg(out.pass ? QStringLiteral("PASS") : QStringLiteral("NG"))
                           .arg(failCodeName(out.failCode)));
}

void LineController::writeRejectToPlc(const ProductRecord &rec)
{
    if (!m_plc || !m_plc->isConnected())
        return;
    m_plc->writeUInt16(QStringLiteral("Result_Slot"), rec.slot);
    m_plc->writeBool(QStringLiteral("PassFail"), rec.pass);
    m_plc->writeUInt16(QStringLiteral("FailCode"), static_cast<quint16>(rec.failCode));
    m_plc->writeReal(QStringLiteral("Measure_D1"), static_cast<float>(rec.od.d1Mm));
    m_plc->writeReal(QStringLiteral("Measure_D2"), static_cast<float>(rec.od.d2Mm));
    m_plc->writeReal(QStringLiteral("Measure_L"), static_cast<float>(rec.length.lengthMm));
    m_plc->writeUInt16(QStringLiteral("DefectGrade"), static_cast<quint16>(rec.defect.grade));
    m_plc->writeBool(QStringLiteral("Result_Ready"), true);
    if (usingMockPlc())
        m_plc->writeBool(QStringLiteral("Result_Ready"), false);
}

void LineController::simulateOneCycle(quint16 slot)
{
    {
        QString err;
        if (!m_algo.prepare(m_projectRoot, m_calibDir, &err)) {
            emit alarm(QStringLiteral("Algo prepare failed: %1").arg(err));
            return;
        }
    }
    if (!m_plc || !m_plc->isConnected()) {
        QString err;
        if (!connectPlc(&err)) {
            emit alarm(err);
            return;
        }
    }

    const quint32 seq = static_cast<quint32>(slot);
    m_plc->writeUInt16(QStringLiteral("Slot_ST1"), slot);
    m_plc->writeUInt16(QStringLiteral("Slot_ST2"), slot);
    m_plc->writeUInt16(QStringLiteral("Slot_ST3"), slot);
    m_plc->writeUInt16(QStringLiteral("Slot_Judge"), slot);

    // Collect frames without going through async queue, then process blocking.
    disconnect(&m_capture, &CaptureDispatcher::frameReady, this, nullptr);
    QList<FramePacket> frames;
    connect(&m_capture, &CaptureDispatcher::frameReady, this,
            [&](const FramePacket &f) { frames.append(f); }, Qt::DirectConnection);

    m_capture.triggerStation(StationId::ST1_Od, slot, seq);
    m_capture.triggerStation(StationId::ST2_Length, slot, seq);
    m_capture.triggerStation(StationId::ST3_Defect, slot, seq);

    disconnect(&m_capture, &CaptureDispatcher::frameReady, this, nullptr);
    wireCaptureToAlgo();

    armSlotDeadline(slot);
    for (const FramePacket &f : frames)
        m_algo.submitFrameBlocking(f);

    ProductRecord rec;
    if (!m_tracker.tryJudge(slot, &rec))
        m_tracker.forceTimeoutJudge(slot, &rec);

    emit statusMessage(QStringLiteral("Simulated cycle slot %1").arg(slot));
}
