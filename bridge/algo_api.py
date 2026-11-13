"""
Stable algorithm API for C++ Qt embedding.

No UI. Frame in (BGR bytes) → track / mask meta out.
Also: detonator measure_od / measure_length / inspect_defect.
"""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any

import numpy as np

from src.inspect_defect import inspect_defect as _inspect_defect
from src.measure_length import measure_length as _measure_length
from src.measure_od import measure_od as _measure_od
from src.pipeline import MODE_BRIGHT, MODE_FACE, DetectTrackPipeline

_pipeline: DetectTrackPipeline | None = None
_grab_cap = None  # cv2.VideoCapture when using Python I/O shim
_grab_is_camera = False
_calib: dict[str, dict[str, Any]] = {}


def load_calib(station: str, path: str) -> str:
    """Load calib JSON for station key ST1|ST2|ST3. Returns station key."""
    key = str(station).upper()
    with open(path, "r", encoding="utf-8") as f:
        _calib[key] = json.load(f)
    return key


def load_calib_dir(calib_dir: str) -> list[str]:
    """Load st1_od.json / st2_length.json / st3_defect.json if present."""
    root = Path(calib_dir)
    mapping = {
        "ST1": "st1_od.json",
        "ST2": "st2_length.json",
        "ST3": "st3_defect.json",
    }
    loaded: list[str] = []
    for key, name in mapping.items():
        p = root / name
        if p.is_file():
            load_calib(key, str(p))
            loaded.append(key)
    return loaded


def _bgr_from_bytes(bgr_bytes: bytes, h: int, w: int, channels: int = 3) -> np.ndarray:
    expected = int(h) * int(w) * int(channels)
    if len(bgr_bytes) != expected:
        raise ValueError(f"buffer size {len(bgr_bytes)} != {expected} ({h}x{w}x{channels})")
    arr = np.frombuffer(bgr_bytes, dtype=np.uint8).reshape(int(h), int(w), int(channels))
    return np.ascontiguousarray(arr.copy())


def measure_od(bgr_bytes: bytes, h: int, w: int, channels: int = 3) -> dict[str, Any]:
    bgr = _bgr_from_bytes(bgr_bytes, h, w, channels)
    return _measure_od(bgr, _calib.get("ST1"))


def measure_length(bgr_bytes: bytes, h: int, w: int, channels: int = 3) -> dict[str, Any]:
    bgr = _bgr_from_bytes(bgr_bytes, h, w, channels)
    return _measure_length(bgr, _calib.get("ST2"))


def inspect_defect(bgr_bytes: bytes, h: int, w: int, channels: int = 3) -> dict[str, Any]:
    bgr = _bgr_from_bytes(bgr_bytes, h, w, channels)
    return _inspect_defect(bgr, _calib.get("ST3"))


def init(
    threshold: int = 180,
    gate_size: int = 80,
    min_pixels: int = 12,
    max_lost: int = 15,
    mode: str = MODE_BRIGHT,
) -> str:
    """Create / replace the global pipeline. Returns backend name."""
    global _pipeline
    _pipeline = DetectTrackPipeline(
        threshold=int(threshold),
        gate_size=int(gate_size),
        min_pixels=int(min_pixels),
        max_lost=int(max_lost),
        mode=mode,
    )
    return _pipeline.backend


def reset() -> None:
    _ensure().tracker.reset()


def set_threshold(v: int) -> None:
    _ensure().detector.threshold = int(max(0, min(255, v)))


def set_gate(v: int) -> None:
    _ensure().tracker.gate_size = int(max(8, v))


def set_mode(mode: str) -> str:
    """mode: 'bright' | 'face'. Resets tracker. Returns active mode."""
    m = MODE_FACE if str(mode).lower() in ("face", "1", "人脸") else MODE_BRIGHT
    return _ensure().set_mode(m)


def process(bgr_bytes: bytes, h: int, w: int, channels: int = 3) -> dict[str, Any]:
    """
    Process one BGR frame.

    Returns dict:
      locked (bool), x, y, miss_x, miss_y (float), lost (int),
      backend (str), mask (bytes, uint8 HxW), mask_h, mask_w
    """
    pipe = _ensure()
    expected = int(h) * int(w) * int(channels)
    if len(bgr_bytes) != expected:
        raise ValueError(f"buffer size {len(bgr_bytes)} != {expected} ({h}x{w}x{channels})")

    arr = np.frombuffer(bgr_bytes, dtype=np.uint8).reshape(int(h), int(w), int(channels))
    # Copy so numpy owns memory after caller frees the buffer
    bgr = np.ascontiguousarray(arr.copy())
    result = pipe.process(bgr)
    tr = result.track
    mask = np.ascontiguousarray(result.mask)

    return {
        "locked": bool(tr.locked),
        "x": float(tr.x),
        "y": float(tr.y),
        "miss_x": float(tr.miss_x),
        "miss_y": float(tr.miss_y),
        "lost": int(tr.lost_frames),
        "backend": str(result.backend),
        "mask": mask.tobytes(),
        "mask_h": int(mask.shape[0]),
        "mask_w": int(mask.shape[1]),
        "threshold": int(pipe.detector.threshold),
        "gate": int(pipe.tracker.gate_size),
        "mode": str(result.mode),
        "det_x": float(result.detection.x) if result.detection else -1.0,
        "det_y": float(result.detection.y) if result.detection else -1.0,
    }


def _ensure() -> DetectTrackPipeline:
    global _pipeline
    if _pipeline is None:
        init()
    assert _pipeline is not None
    return _pipeline


def _open_camera_backend(idx: int, backend: int, timeout_sec: float = 3.0):
    """Open one backend in a daemon thread so a hung VideoCapture cannot freeze Qt."""
    import threading
    import cv2

    box: dict = {"cap": None}

    def worker() -> None:
        cap = None
        try:
            cap = cv2.VideoCapture(idx, backend)
            if not cap.isOpened():
                return
            try:
                cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)
                cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
                cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)
            except Exception:
                pass
            ok, frame = cap.read()
            if ok and frame is not None:
                box["cap"] = cap
                cap = None  # ownership transferred
        except Exception:
            pass
        finally:
            if cap is not None:
                try:
                    cap.release()
                except Exception:
                    pass

    t = threading.Thread(target=worker, daemon=True)
    t.start()
    t.join(timeout=timeout_sec)
    if t.is_alive():
        # Backend hung; abandon thread (daemon). Do not block UI.
        return None
    return box["cap"]


def grab_open(source: str) -> bool:
    """Open camera index or video path via OpenCV (I/O shim for Qt without C++ OpenCV)."""
    global _grab_cap, _grab_is_camera
    grab_close()
    import cv2
    import sys

    if source.isdigit():
        idx = int(source)
        # DSHOW first: MSMF often hangs on open on some Win machines
        if sys.platform == "win32":
            backends = [cv2.CAP_DSHOW, cv2.CAP_MSMF, cv2.CAP_ANY]
        else:
            backends = [cv2.CAP_ANY]
        for be in backends:
            cap = _open_camera_backend(idx, be, timeout_sec=3.0)
            if cap is not None:
                _grab_cap = cap
                _grab_is_camera = True
                return True
        return False

    cap = cv2.VideoCapture(source)
    if not cap.isOpened():
        return False
    _grab_cap = cap
    _grab_is_camera = False
    return True


def grab_read() -> tuple[bytes, int, int] | None:
    """Return (bgr_bytes, h, w) or None on failure/EOF. Loops video to start."""
    global _grab_cap
    if _grab_cap is None:
        return None

    # Single read only — multi-grab was blocking the worker and freezing the UI
    ok, frame = _grab_cap.read()

    if not ok or frame is None:
        if _grab_is_camera:
            return None
        # rewind video sources
        _grab_cap.set(cv2.CAP_PROP_POS_FRAMES, 0)
        ok, frame = _grab_cap.read()
        if not ok or frame is None:
            return None
    if frame.ndim != 3 or frame.shape[2] != 3:
        return None
    h, w = int(frame.shape[0]), int(frame.shape[1])
    bgr = np.ascontiguousarray(frame)
    return bgr.tobytes(), h, w


def grab_close() -> None:
    global _grab_cap, _grab_is_camera
    if _grab_cap is not None:
        try:
            _grab_cap.release()
        except Exception:
            pass
        _grab_cap = None
    _grab_is_camera = False
