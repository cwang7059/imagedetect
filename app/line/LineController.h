#pragma once

#include "algo/AlgoDispatcher.h"
#include "camera/CaptureDispatcher.h"
#include "camera/HikLineScanCamera.h"
#include "camera/OnnaLineScanCamera.h"
#include "core/FinalJudge.h"
#include "core/ProductTracker.h"
#include "core/Recipe.h"
#include "line/ResultTrace.h"
#include "plc/IPlcClient.h"
#include "plc/MockPlcClient.h"
#include "plc/PlcPointMap.h"
#include "plc/S7Client.h"

#include <QHash>
#include <QObject>
#include <QTimer>

/**
 * Production line orchestrator:
 * poll PLC → camera → algo threads → tracker → final judge → PLC reject.
 * Per-slot wall-clock timeout forces NG if algorithm lags behind the fixture.
 */
class LineController : public QObject
{
    Q_OBJECT
public:
    explicit LineController(QObject *parent = nullptr);
    ~LineController() override;

    bool initialize(const QString &projectRoot, QString *error = nullptr);

    ProductTracker *tracker() { return &m_tracker; }
    ResultTrace *trace() { return &m_trace; }
    const Recipe &recipe() const { return m_recipe; }
    bool setRecipePath(const QString &path, QString *error = nullptr);

    bool plcConnected() const;
    bool usingMockPlc() const;

public slots:
    void startLine();
    void stopLine();
    bool connectPlc(QString *error = nullptr);
    void disconnectPlc();
    bool runLoopbackTest(QString *error = nullptr);
    bool runSelfCheck(QString *error = nullptr);

    /** Dry-run: blocking algo submit so results are ready before judge. */
    void simulateOneCycle(quint16 slot = 1);

    /** Soft-grab all station cameras for UI only (no algo / no judge). */
    void setLivePreviewEnabled(bool on);
    bool livePreviewEnabled() const { return m_livePreview; }
    void refreshLivePreview();

signals:
    void statusMessage(const QString &msg);
    void alarm(const QString &msg);
    void lineRunningChanged(bool running);
    void plcStateChanged(bool connected, const QString &detail);
    void statsChanged(int ok, int ng);
    void previewFrame(const FramePacket &frame);
    void recordJudged(const ProductRecord &rec);
    void recordUpdated(const ProductRecord &rec);

private slots:
    void onPoll();
    void onHeartbeat();
    void onSlotTimeoutScan();
    void onLivePreviewTick();
    void onOd(quint16 slot, quint32 seq, const StationOdResult &r);
    void onLength(quint16 slot, quint32 seq, const StationLengthResult &r);
    void onDefect(quint16 slot, quint32 seq, const StationDefectResult &r);
    void onJudged(const ProductRecord &rec);
    void onPreviewCache(const FramePacket &frame);
    void writeRejectToPlc(const ProductRecord &rec);
    QString maybeSaveNgImage(ProductRecord *rec);

private:
    void wireCaptureToAlgo();
    void clearTrigEdge(const QString &trigName);
    bool edgeTrig(const QString &trigName, bool *rising);
    void armSlotDeadline(quint16 slot);
    void clearSlotDeadline(quint16 slot);
    static quint32 frameKey(quint16 slot, StationId st);

    QString m_projectRoot;
    QString m_calibDir;
    PlcPointMap m_points;
    S7Client m_s7;
    IPlcClient *m_plc = nullptr;

    HikLineScanCamera *m_camOd = nullptr;
    HikLineScanCamera *m_camLen = nullptr;
    OnnaLineScanCamera *m_camDef = nullptr;
    CaptureDispatcher m_capture;
    AlgoDispatcher m_algo;
    FinalJudge m_judge;
    ProductTracker m_tracker;
    Recipe m_recipe;
    ResultTrace m_trace;

    QTimer m_pollTimer;
    QTimer m_hbTimer;
    QTimer m_slotTimeoutTimer;
    QTimer m_livePreviewTimer;
    bool m_running = false;
    bool m_livePreview = false;
    bool m_previewOnly = false; // true while soft-grabbing for UI
    bool m_prevTrig[4] = {false, false, false, false};
    bool m_pcAlive = false;
    QHash<quint32, FramePacket> m_lastFrames;
    QHash<quint16, qint64> m_slotDeadlineMs; // slot → deadline epoch ms
};
