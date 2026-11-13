"""Surface defect inspection for Onna line-scan (ST3)."""

from __future__ import annotations

from typing import Any

import numpy as np


def _gray(bgr: np.ndarray) -> np.ndarray:
    return (
        0.114 * bgr[:, :, 0] + 0.587 * bgr[:, :, 1] + 0.299 * bgr[:, :, 2]
    ).astype(np.uint8)


def _box_blur(gray: np.ndarray, k: int) -> np.ndarray:
    """Mean filter via integral image; output same shape as input."""
    k = max(1, int(k) | 1)
    pad = k // 2
    g = gray.astype(np.float64)
    padded = np.pad(g, ((pad, pad), (pad, pad)), mode="edge")
    # integral with zero border
    integral = np.pad(padded, ((1, 0), (1, 0)), mode="constant")
    np.cumsum(integral, axis=0, out=integral)
    np.cumsum(integral, axis=1, out=integral)
    h, w = g.shape
    # window sum for each top-left at (i,j) covering kxk ending at (i+k-1, j+k-1)
    # in padded coords top-left is (i, j), bottom-right exclusive (i+k, j+k)
    s00 = integral[0:h, 0:w]
    s01 = integral[0:h, k : k + w]
    s10 = integral[k : k + h, 0:w]
    s11 = integral[k : k + h, k : k + w]
    out = (s11 - s01 - s10 + s00) / float(k * k)
    return out.astype(np.float32)


def inspect_defect(bgr: np.ndarray, calib: dict[str, Any] | None = None) -> dict[str, Any]:
    calib = calib or {}
    thr = int(calib.get("threshold", 45))
    min_area = int(calib.get("min_blob_area", 40))
    bins = list(calib.get("grade_area_bins", [80, 200, 500]))
    blur_k = int(calib.get("blur_ksize", 3))
    roi = calib.get("roi")
    img = bgr
    if roi and len(roi) == 4:
        x, y, rw, rh = [int(v) for v in roi]
        img = bgr[y : y + rh, x : x + rw]

    gray_u8 = _gray(img)
    gray = gray_u8.astype(np.float32)
    blur = _box_blur(gray_u8, blur_k)
    diff = np.abs(gray - blur)
    mask = diff >= thr

    m = mask.astype(np.uint8)
    h, w = m.shape
    visited = np.zeros_like(m, dtype=np.uint8)
    areas: list[int] = []
    for y in range(h):
        for x in range(w):
            if m[y, x] == 0 or visited[y, x]:
                continue
            stack = [(y, x)]
            visited[y, x] = 1
            area = 0
            while stack:
                cy, cx = stack.pop()
                area += 1
                for ny, nx in ((cy - 1, cx), (cy + 1, cx), (cy, cx - 1), (cy, cx + 1)):
                    if 0 <= ny < h and 0 <= nx < w and m[ny, nx] and not visited[ny, nx]:
                        visited[ny, nx] = 1
                        stack.append((ny, nx))
            if area >= min_area:
                areas.append(area)

    grade = 0
    max_area = max(areas) if areas else 0
    for i, edge in enumerate(bins):
        if max_area >= int(edge):
            grade = i + 1
    return {
        "ok": True,
        "grade": int(grade),
        "defect_count": int(len(areas)),
        "max_area": int(max_area),
        "summary": f"defects={len(areas)} max_area={max_area} grade={grade}",
    }
