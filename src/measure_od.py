"""Outer-diameter measurement for cylindrical detonator ends (ST1)."""

from __future__ import annotations

from typing import Any

import numpy as np


def _gray(bgr: np.ndarray) -> np.ndarray:
    return (
        0.114 * bgr[:, :, 0] + 0.587 * bgr[:, :, 1] + 0.299 * bgr[:, :, 2]
    ).astype(np.uint8)


def _diameter_at_x(gray: np.ndarray, x: int, band: int, thr: int, min_pix: int) -> float:
    h, w = gray.shape
    x0 = max(0, x - band)
    x1 = min(w, x + band + 1)
    col = gray[:, x0:x1].mean(axis=1)
    mask = col >= thr
    if int(mask.sum()) < min_pix:
        return 0.0
    ys = np.flatnonzero(mask)
    return float(ys[-1] - ys[0] + 1)


def measure_od(bgr: np.ndarray, calib: dict[str, Any] | None = None) -> dict[str, Any]:
    calib = calib or {}
    mm_per_px = float(calib.get("mm_per_pixel", 0.02))
    thr = int(calib.get("threshold", 90))
    band = int(calib.get("band_half_width_px", 24))
    min_pix = int(calib.get("min_edge_pixels", 8))
    roi = calib.get("roi")
    img = bgr
    if roi and len(roi) == 4:
        x, y, rw, rh = [int(v) for v in roi]
        img = bgr[y : y + rh, x : x + rw]
    gray = _gray(img)
    h, w = gray.shape
    xa = int(float(calib.get("end_a_x_ratio", 0.18)) * w)
    xb = int(float(calib.get("end_b_x_ratio", 0.82)) * w)
    d1_px = _diameter_at_x(gray, xa, band, thr, min_pix)
    d2_px = _diameter_at_x(gray, xb, band, thr, min_pix)
    d1 = d1_px * mm_per_px
    d2 = d2_px * mm_per_px
    ok = d1_px > 0 and d2_px > 0
    score = 1.0 if ok else 0.0
    if ok:
        score = max(0.0, 1.0 - abs(d1 - d2) / max(d1, d2, 1e-6))
    return {
        "ok": bool(ok),
        "d1_mm": float(d1),
        "d2_mm": float(d2),
        "d1_px": float(d1_px),
        "d2_px": float(d2_px),
        "score": float(score),
    }
