#pragma once

#include "CaptureWorker.h"
#include "camera/FramePacket.h"
#include "core/ProductRecord.h"
#include "core/StationTypes.h"
#include "line/LineController.h"

#include <QCheckBox>
#include <QLabel>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QThread>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(const QString &projectRoot, QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onFrame(const QImage &vis, const QImage &mask, const TrackMeta &meta, double fps);
    void onStatus(const QString &msg);
    void onFailed(const QString &err);
    void onPrepared();
    void onSourceChanged(const QString &label);

    void onSynth();
    void onCamera();
    void onVideo();
    void onReset();
    void onModeChanged(int index);
    void onThreshold(int v);
    void onGate(int v);

    void onLineConnect();
    void onLineStart();
    void onLineStop();
    void onLineSelfCheck();
    void onLineSimulate();
    void onLineLoopback();
    void onLineStatus(const QString &msg);
    void onLineAlarm(const QString &msg);
    void onLineStats(int ok, int ng);
    void onLineJudged(const ProductRecord &rec);
    void onLinePreview(const FramePacket &frame);
    void onPlcState(bool connected, const QString &detail);
    void onLivePreviewToggled(bool on);

private:
    void setupWorker();
    void setupLineTab();
    void initLineController();
    void appendLineLog(const QString &msg);
    void loadOdPreviewCalib();
    QLabel *makeStationPreviewLabel(const QString &title, QWidget *parent);
    void showImageOnLabel(QLabel *label, const QImage &bgrOrRgb, bool isBgr);
    void showOdEndCrops(const FramePacket &frame);

    Ui::MainWindow *ui = nullptr;
    QString m_projectRoot;
    QString m_sourceLabel = QStringLiteral("合成源");
    QThread m_thread;
    CaptureWorker *m_worker = nullptr;

    LineController *m_line = nullptr;
    /** ST1 两端圆形外径：端 A=D1、端 B=D2；与长度/瑕疵同嵌在主客户端 */
    QLabel *m_previewOdA = nullptr;
    QLabel *m_previewOdB = nullptr;
    QLabel *m_previewLen = nullptr;
    QLabel *m_previewDef = nullptr;
    QLabel *m_lineStats = nullptr;
    QLabel *m_linePlc = nullptr;
    QPlainTextEdit *m_lineLog = nullptr;
    QTableWidget *m_lineTable = nullptr;
    QPushButton *m_btnLineStart = nullptr;
    QPushButton *m_btnLineStop = nullptr;
    QCheckBox *m_chkLivePreview = nullptr;
    int m_simSlot = 1;

    double m_odEndARatio = 0.18;
    double m_odEndBRatio = 0.82;
    int m_odCropHalfWidth = 48;
};
