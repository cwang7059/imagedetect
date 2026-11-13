#pragma once

#include "PythonBridge.h"

#include <QAtomicInt>
#include <QElapsedTimer>
#include <QImage>
#include <QObject>
#include <QString>
#include <QTimer>

class CaptureWorker : public QObject
{
    Q_OBJECT
public:
    enum SourceMode {
        Synth = 0,
        Camera = 1,
        Video = 2
    };
    Q_ENUM(SourceMode)

    explicit CaptureWorker(QObject *parent = nullptr);

public slots:
    void prepare(const QString &projectRoot, int threshold, int gate);
    void startCapture();
    void stopCapture();
    void setSourceSynth();
    void setSourceCamera(int index);
    void setSourceVideo(const QString &path);
    void setThreshold(int v);
    void setGate(int v);
    void setDetectMode(const QString &mode); // "bright" | "face"
    void resetTrack();

signals:
    void frameReady(const QImage &visRgb, const QImage &maskRgb, const TrackMeta &meta, double fps);
    void statusMessage(const QString &msg);
    void failed(const QString &error);
    void prepared();
    void sourceChanged(const QString &sourceLabel);

private slots:
    void tick();

private:
    QByteArray makeSynthFrame(int w, int h, double tSec);
    QImage bgrToRgbImage(const uchar *bgr, int h, int w) const;
    QImage maskToRgbImage(const QByteArray &mask, int h, int w) const;

    void updateTimerInterval();

    PythonBridge m_bridge;
    QTimer *m_timer = nullptr;
    SourceMode m_mode = Synth;
    QString m_videoPath;
    int m_cameraIndex = 0;
    QElapsedTimer m_clock;
    QElapsedTimer m_frameClock;
    double m_fpsEma = 0.0;
    int m_frameId = 0;
    bool m_ready = false;
    bool m_busy = false; // drop ticks if previous still running
};
