#include "SimLineScanCamera.h"

#include <QDateTime>
#include <QtMath>

SimLineScanCamera::SimLineScanCamera(StationId station, const QString &id, QObject *parent)
    : ICamera(parent)
    , m_station(station)
    , m_id(id)
{
}

void SimLineScanCamera::setImageSize(int w, int h)
{
    m_w = qMax(64, w);
    m_h = qMax(32, h);
}

bool SimLineScanCamera::openCamera(QString *error)
{
    Q_UNUSED(error);
    m_open = true;
    emit onlineChanged(true);
    return true;
}

void SimLineScanCamera::closeCamera()
{
    m_open = false;
    emit onlineChanged(false);
}

QByteArray SimLineScanCamera::makeFrame(quint16 slot) const
{
    QByteArray buf(m_w * m_h * 3, 28);
    const int cy = m_h / 2;
    const int diameterPx = 40 + (slot % 5);

    auto put = [&](int x, int y, uchar b, uchar g, uchar r) {
        if (x < 0 || x >= m_w || y < 0 || y >= m_h)
            return;
        uchar *p = reinterpret_cast<uchar *>(buf.data()) + (y * m_w + x) * 3;
        p[0] = b;
        p[1] = g;
        p[2] = r;
    };

    if (m_station == StationId::ST1_Od) {
        // 外径：两端圆形截面（模拟端面相机视野落在整幅线扫图左右）
        const int r = diameterPx / 2;
        const int cxA = int(0.18 * m_w);
        const int cxB = int(0.82 * m_w);
        for (int end = 0; end < 2; ++end) {
            const int cx = (end == 0) ? cxA : cxB;
            for (int y = cy - r - 2; y <= cy + r + 2; ++y) {
                for (int x = cx - r - 2; x <= cx + r + 2; ++x) {
                    const int dx = x - cx;
                    const int dy = y - cy;
                    const int d2 = dx * dx + dy * dy;
                    if (d2 <= r * r) {
                        const int shade = 210 - d2 / qMax(1, r);
                        put(x, y, uchar(qBound(60, shade, 255)), uchar(qBound(60, shade, 255)),
                            uchar(qBound(60, shade, 255)));
                    } else if (d2 <= (r + 2) * (r + 2)) {
                        put(x, y, 20, 180, 40); // 轮廓描边，便于确认圆形截面
                    }
                }
            }
        }
        return buf;
    }

    // 长度 / 瑕疵：圆柱侧视亮条
    const int halfH = qMax(8, m_h / 8);
    const int left = m_w / 8;
    const int right = m_w - m_w / 8;
    for (int y = cy - halfH; y <= cy + halfH; ++y) {
        if (y < 0 || y >= m_h)
            continue;
        for (int x = left; x < right; ++x) {
            const int bright = 220 - qAbs(y - cy) * 4;
            put(x, y, uchar(qBound(40, bright, 255)), uchar(qBound(40, bright, 255)),
                uchar(qBound(40, bright, 255)));
        }
    }
    if (m_station == StationId::ST3_Defect && (slot % 7) == 0) {
        for (int y = cy - 6; y <= cy + 6; ++y) {
            for (int x = m_w / 2 - 8; x <= m_w / 2 + 8; ++x)
                put(x, y, 10, 10, 200);
        }
    }
    return buf;
}

bool SimLineScanCamera::onHardTrigger(quint16 slot, quint32 productSeq, QString *error)
{
    if (!m_open) {
        if (error)
            *error = QStringLiteral("Camera %1 not open").arg(m_id);
        return false;
    }
    FramePacket pkt;
    pkt.station = m_station;
    pkt.slot = slot;
    pkt.productSeq = productSeq;
    pkt.timestampMs = QDateTime::currentMSecsSinceEpoch();
    pkt.width = m_w;
    pkt.height = m_h;
    pkt.channels = 3;
    pkt.buffer = makeFrame(slot);
    pkt.cameraId = m_id;
    emit frameReady(pkt);
    return true;
}
