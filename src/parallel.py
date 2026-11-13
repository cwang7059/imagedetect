"""并行计算后端：优先 CUDA，否则 Numba CPU 多核并行。"""

from __future__ import annotations

import numpy as np
from numba import cuda, njit, prange


def backend_info() -> dict:
    cuda_ok = bool(cuda.is_available())
    return {
        "backend": "cuda" if cuda_ok else "numba_cpu",
        "cuda_available": cuda_ok,
    }


@njit(parallel=True, cache=True)
def _gray_parallel(bgr: np.ndarray, out: np.ndarray) -> None:
    h, w, _ = bgr.shape
    for y in prange(h):
        for x in range(w):
            b = float(bgr[y, x, 0])
            g = float(bgr[y, x, 1])
            r = float(bgr[y, x, 2])
            out[y, x] = np.uint8(0.114 * b + 0.587 * g + 0.299 * r)


@njit(parallel=True, cache=True)
def _threshold_mask(gray: np.ndarray, thr: np.uint8, out: np.ndarray) -> None:
    h, w = gray.shape
    for y in prange(h):
        for x in range(w):
            out[y, x] = 255 if gray[y, x] >= thr else 0


@njit(parallel=True, cache=True)
def _box_blur3(src: np.ndarray, out: np.ndarray) -> None:
    """3x3 盒式滤波（边界复制）。"""
    h, w = src.shape
    for y in prange(h):
        y0 = 0 if y == 0 else y - 1
        y1 = h - 1 if y == h - 1 else y + 1
        for x in range(w):
            x0 = 0 if x == 0 else x - 1
            x1 = w - 1 if x == w - 1 else x + 1
            s = (
                int(src[y0, x0])
                + int(src[y0, x])
                + int(src[y0, x1])
                + int(src[y, x0])
                + int(src[y, x])
                + int(src[y, x1])
                + int(src[y1, x0])
                + int(src[y1, x])
                + int(src[y1, x1])
            )
            out[y, x] = np.uint8(s // 9)


# ---- 可选 CUDA kernel（有 NVIDIA GPU 时自动可用）----

if cuda.is_available():

    @cuda.jit
    def _gray_cuda_kernel(bgr, out):
        x, y = cuda.grid(2)
        if y < bgr.shape[0] and x < bgr.shape[1]:
            b = float(bgr[y, x, 0])
            g = float(bgr[y, x, 1])
            r = float(bgr[y, x, 2])
            out[y, x] = np.uint8(0.114 * b + 0.587 * g + 0.299 * r)

    @cuda.jit
    def _threshold_cuda_kernel(gray, thr, out):
        x, y = cuda.grid(2)
        if y < gray.shape[0] and x < gray.shape[1]:
            out[y, x] = 255 if gray[y, x] >= thr else 0


def to_gray(bgr: np.ndarray) -> np.ndarray:
    """BGR -> 灰度，GPU 可用走 CUDA，否则 Numba 并行。"""
    assert bgr.ndim == 3 and bgr.shape[2] == 3
    out = np.empty(bgr.shape[:2], dtype=np.uint8)
    if cuda.is_available():
        d_bgr = cuda.to_device(np.ascontiguousarray(bgr))
        d_out = cuda.device_array(out.shape, dtype=np.uint8)
        threads = (16, 16)
        blocks = (
            (out.shape[1] + threads[0] - 1) // threads[0],
            (out.shape[0] + threads[1] - 1) // threads[1],
        )
        _gray_cuda_kernel[blocks, threads](d_bgr, d_out)
        return d_out.copy_to_host()
    _gray_parallel(np.ascontiguousarray(bgr), out)
    return out


def blur3(gray: np.ndarray) -> np.ndarray:
    out = np.empty_like(gray)
    _box_blur3(np.ascontiguousarray(gray), out)
    return out


def threshold(gray: np.ndarray, thr: int) -> np.ndarray:
    out = np.empty_like(gray)
    t = np.uint8(max(0, min(255, int(thr))))
    if cuda.is_available():
        d_gray = cuda.to_device(np.ascontiguousarray(gray))
        d_out = cuda.device_array(out.shape, dtype=np.uint8)
        threads = (16, 16)
        blocks = (
            (out.shape[1] + threads[0] - 1) // threads[0],
            (out.shape[0] + threads[1] - 1) // threads[1],
        )
        _threshold_cuda_kernel[blocks, threads](d_gray, int(t), d_out)
        return d_out.copy_to_host()
    _threshold_mask(np.ascontiguousarray(gray), t, out)
    return out


@njit(cache=True)
def centroid_from_mask(mask: np.ndarray, min_pixels: int = 8):
    """掩膜质心；像素过少返回 (-1.0, -1.0, 0)。"""
    h, w = mask.shape
    sx = 0.0
    sy = 0.0
    n = 0
    for y in range(h):
        for x in range(w):
            if mask[y, x] != 0:
                sx += x
                sy += y
                n += 1
    if n < min_pixels:
        return -1.0, -1.0, 0
    return sx / n, sy / n, n
