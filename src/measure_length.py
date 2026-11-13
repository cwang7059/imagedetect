"""Length measurement along line-scan axis (ST2)."""

from __future__ import annotations

from typing import Any

import numpy as np


def _gray(bgr: np.ndarray) -> np.ndarray:
    return (
        0.114 * bgr[:, :, 0] + 0.587 * bgr[:, :, 1] + 0.299 * bgr[:, :, 2]
    ).astype(np.uint8)


def measure_length(bgr: np.ndarray, calib: dict[str, Any] | None = None) -> dict[str, Any]:
    calib = calib or {}
    mm_per_px = float(calib.get("mm_per_pixel", 0.05))
    thr = int(calib.get("threshold", 90))
    min_run = int(calib.get("min_run_px", 20))
    roi = calib.get("roi")
    img = bgr
    if roi and len(roi) == 4:
        x, y, rw, rh = [int(v) for v in roi]
        img = bgr[y : y + rh, x : x + rw]
    gray = _gray(img)
    # Use column max so partial-height ROI still sees the cylinder body
    profile = gray.max(axis=0)
    mask = profile >= thr
    if int(mask.sum()) < min_run:
        return {"ok": False, "length_mm": 0.0, "length_px": 0.0}
    xs = np.flatnonzero(mask)
    length_px = float(xs[-1] - xs[0] + 1)
    return {
        "ok": True,
        "length_mm": float(length_px * mm_per_px),
        "length_px": length_px,
    }
