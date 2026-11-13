#include "Overlay.h"

#include <QPainter>
#include <QtMath>

static void drawCross(QPainter *p, int cx, int cy, int arm, const QColor &c)
{
    p->setPen(QPen(c, 1));
    p->drawLine(cx - arm, cy, cx + arm, cy);
    p->drawLine(cx, cy - arm, cx, cy + arm);
}

void drawTrackOverlay(QImage *img, const TrackMeta &meta)
{
    if (!img || img->isNull())
        return;

    QPainter p(img);
    p.setRenderHint(QPainter::Antialiasing, false);

    const int w = img->width();
    const int h = img->height();
    const int cx = w / 2;
    const int cy = h / 2;

    drawCross(&p, cx, cy, 12, QColor(0, 255, 255));

    if (meta.locked) {
        const int half = qMax(4, meta.gate / 2);
        const int gx = qRound(meta.x);
        const int gy = qRound(meta.y);
        QRect gate(gx - half, gy - half, half * 2, half * 2);
        gate = gate.intersected(QRect(0, 0, w, h));
        p.setPen(QPen(QColor(0, 200, 0), 1));
        p.drawRect(gate);

        p.setPen(QPen(QColor(0, 0, 255), 2));
        p.drawEllipse(QPoint(gx, gy), 6, 6);

        p.setPen(QPen(QColor(0, 128, 255), 1));
        p.drawLine(cx, cy, gx, gy);

        p.setPen(QColor(0, 255, 0));
        p.setFont(QFont(QStringLiteral("Segoe UI"), 11, QFont::Bold));
        p.drawText(
            12,
            28,
            QStringLiteral("LOCK miss=(%1,%2) lost=%3")
                .arg(meta.missX, 0, 'f', 1)
                .arg(meta.missY, 0, 'f', 1)
                .arg(meta.lost));
    } else {
        p.setPen(QColor(255, 165, 0));
        p.setFont(QFont(QStringLiteral("Segoe UI"), 11, QFont::Bold));
        p.drawText(12, 28, QStringLiteral("SEARCH"));
        if (meta.detX >= 0.f && meta.detY >= 0.f) {
            p.setPen(QPen(QColor(255, 0, 0), 2));
            p.drawEllipse(QPoint(qRound(meta.detX), qRound(meta.detY)), 5, 5);
        }
    }

    p.setPen(QColor(220, 220, 220));
    p.setFont(QFont(QStringLiteral("Segoe UI"), 9));
    const QString modeTag = (meta.mode == QLatin1String("face"))
        ? QStringLiteral("face")
        : QStringLiteral("thr=%1").arg(meta.threshold);
    p.drawText(
        12,
        56,
        QStringLiteral("backend=%1 %2")
            .arg(meta.backend.isEmpty() ? QStringLiteral("?") : meta.backend)
            .arg(modeTag));
}
