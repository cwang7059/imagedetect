#pragma once

#include "PythonBridge.h"

#include <QImage>

/** Draw track overlay on a BGR888 QImage (in-place). */
void drawTrackOverlay(QImage *bgrImage, const TrackMeta &meta);
