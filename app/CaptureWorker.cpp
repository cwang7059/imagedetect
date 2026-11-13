#include "CaptureWorker.h"
#include "Overlay.h"

#include <QPainter>
#include <QtMath>

CaptureWorker::CaptureWorker(QObject *parent)
    : QObject(parent)
{
    m_timer = new QTimer(this);
    m_timer->setTimerType(Qt::PreciseTimer);
    m_timer->setInterval(33); // ~30 FPS; camera must not spin at 1ms
    connect(m_timer, &QTimer::timeout, this, &CaptureWorker::tick);
}

void CaptureWorker::updateTimerInterval()
{
    // Synth can run a bit faster; camera/video stay near sensor rate
    m_timer->setInterval(m_mode == Synth ? 16 : 33);
}

void CaptureWorker::prepare(const QString &projectRoot, int threshold, int gate)
{
    QString err;
    if (!m_bridge.initialize(projectRoot, &err)) {
        emit failed(err);
        return;
    }
    if (!m_bridge.initPipeline(threshold, gate, &err)) {
        emit failed(err);
        return;
    }
    m_ready = true;
    m_clock.start();
    m_fpsEma = 0.0;
    m_frameId = 0;
    emit statusMessage(QStringLiteral("pipeline ready"));
    emit prepared();
}

void CaptureWorker::startCapture()
{
    if (!m_ready)
        return;
    if (!m_timer->isActive())
        m_timer->start();
}

void CaptureWorker::stopCapture()
{
    m_timer->stop();
    m_bridge.grabClose();
}

void CaptureWorker::setSourceSynth()
{
    m_bridge.grabClose();
    m_mode = Synth;
    updateTimerInterval();
    emit sourceChanged(QStringLiteral("合成源"));
    emit statusMessage(QStringLiteral("source: synth"));
}

void CaptureWorker::setSourceCamera(int index)
{
    m_cameraIndex = index;
    const bool wasRunning = m_timer->isActive();
    m_timer->stop(); // avoid ticks while open blocks
    emit statusMessage(QStringLiteral("正在打开摄像头 %1 …").arg(index));

    QString err;
    if (!m_bridge.grabOpen(QString::number(index), &err)) {
        m_mode = Synth;
        updateTimerInterval();
        emit sourceChanged(QStringLiteral("合成源"));
        emit statusMessage(QStringLiteral("摄像头打开失败/超时，已回退合成源"));
        if (wasRunning)
            m_timer->start();
        return;
    }
    m_mode = Camera;
    updateTimerInterval();
    m_frameId = 0;
    m_frameClock.invalidate();
    emit sourceChanged(QStringLiteral("摄像头%1").arg(index));
    emit statusMessage(QStringLiteral("摄像头已打开"));
    if (wasRunning)
        m_timer->start();
}

void CaptureWorker::setSourceVideo(const QString &path)
{
    QString err;
    if (!m_bridge.grabOpen(path, &err)) {
        m_mode = Synth;
        updateTimerInterval();
        emit sourceChanged(QStringLiteral("合成源"));
        emit statusMessage(QStringLiteral("视频打开失败: %1（已回退合成源）").arg(err));
        return;
    }
    m_videoPath = path;
    m_mode = Video;
    updateTimerInterval();
    m_frameId = 0;
    m_frameClock.invalidate();
    emit sourceChanged(QStringLiteral("视频"));
    emit statusMessage(QStringLiteral("source: video"));
}

void CaptureWorker::setThreshold(int v)
{
    QString err;
    m_bridge.setThreshold(v, &err);
}

void CaptureWorker::setGate(int v)
{
    QString err;
    m_bridge.setGate(v, &err);
}

void CaptureWorker::setDetectMode(const QString &mode)
{
    QString err;
    if (!m_bridge.setMode(mode, &err)) {
        emit statusMessage(QStringLiteral("切换模式失败: %1").arg(err));
        return;
    }
    const bool face = (mode == QLatin1String("face"));
    emit statusMessage(face ? QStringLiteral("检测模式: 人脸") : QStringLiteral("检测模式: 亮斑"));
}

void CaptureWorker::resetTrack()
{
    QString err;
    m_bridge.reset(&err);
    emit statusMessage(QStringLiteral("track reset"));
}

QByteArray CaptureWorker::makeSynthFrame(int w, int h, double tSec)
{
    QByteArray buf(w * h * 3, 0);
    uchar *p = reinterpret_cast<uchar *>(buf.data());
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const int i = (y * w + x) * 3;
            const int n = (x * 13 + y * 7 + int(tSec * 30)) & 15;
            p[i + 0] = uchar(18 + n);
            p[i + 1] = uchar(18 + n);
            p[i + 2] = uchar(22 + n);
        }
    }
    const int cx = int(w * 0.5 + 180.0 * qSin(tSec * 1.3));
    const int cy = int(h * 0.5 + 110.0 * qCos(tSec * 0.9));
    const int r = 7;
    for (int y = cy - r; y <= cy + r; ++y) {
        for (int x = cx - r; x <= cx + r; ++x) {
            if (x < 0 || y < 0 || x >= w || y >= h)
                continue;
            if ((x - cx) * (x - cx) + (y - cy) * (y - cy) > r * r)
                continue;
            const int i = (y * w + x) * 3;
            p[i] = p[i + 1] = p[i + 2] = 255;
        }
    }
    return buf;
}

QImage CaptureWorker::bgrToRgbImage(const uchar *bgr, int h, int w) const
{
    // Qt 5.14+: BGR888 avoids per-pixel swap; copy so buffer outlives emit
    QImage img(bgr, w, h, w * 3, QImage::Format_BGR888);
    return img.copy();
}

QImage CaptureWorker::maskToRgbImage(const QByteArray &mask, int h, int w) const
{
    QImage img(w, h, QImage::Format_RGB888);
    img.fill(Qt::black);
    if (mask.size() < h * w)
        return img;
    const uchar *m = reinterpret_cast<const uchar *>(mask.constData());
    for (int y = 0; y < h; ++y) {
        uchar *dst = img.scanLine(y);
        for (int x = 0; x < w; ++x) {
            const uchar v = m[y * w + x];
            dst[x * 3 + 0] = v;
            dst[x * 3 + 1] = v;
            dst[x * 3 + 2] = v;
        }
    }
    return img;
}

void CaptureWorker::tick()
{
    if (!m_ready || m_busy)
        return;
    m_busy = true;

    QByteArray bgr;
    int h = 0;
    int w = 0;

    if (m_mode == Synth) {
        w = 640;
        h = 480;
        bgr = makeSynthFrame(w, h, m_clock.elapsed() / 1000.0);
    } else {
        QString err;
        if (!m_bridge.grabRead(&bgr, &h, &w, &err) || bgr.isEmpty()) {
            m_busy = false;
            return;
        }
    }

    TrackMeta meta;
    QString err;
    if (!m_bridge.process(reinterpret_cast<const uchar *>(bgr.constData()), h, w, &meta, &err)) {
        emit statusMessage(QStringLiteral("process failed: %1").arg(err));
        m_busy = false;
        return;
    }

    // Wall-clock display FPS (not process-only, which was falsely 600+)
    double fps = 0.0;
    if (m_frameClock.isValid()) {
        const double dtMs = m_frameClock.nsecsElapsed() / 1e6;
        if (dtMs > 1e-3)
            fps = 1000.0 / dtMs;
    }
    m_frameClock.restart();
    m_fpsEma = (m_frameId == 0 || fps <= 0.0) ? fps : (m_fpsEma * 0.85 + fps * 0.15);
    ++m_frameId;

    QImage vis = bgrToRgbImage(reinterpret_cast<const uchar *>(bgr.constData()), h, w);
    drawTrackOverlay(&vis, meta);
    {
        QPainter p(&vis);
        p.setPen(QColor(200, 255, 200));
        p.setFont(QFont(QStringLiteral("Segoe UI"), 9));
        p.drawText(12, 84, QStringLiteral("FPS %1").arg(m_fpsEma, 0, 'f', 1));
    }

    const QImage maskImg = maskToRgbImage(
        meta.mask, meta.maskH > 0 ? meta.maskH : h, meta.maskW > 0 ? meta.maskW : w);
    emit frameReady(vis, maskImg, meta, m_fpsEma);
    m_busy = false;
}
