"""检测 + 跟踪流水线。"""

from __future__ import annotations

from dataclasses import dataclass

import cv2
import numpy as np

from . import parallel
from .detector import BrightSpotDetector, Detection
from .face_detector import FaceDetector
from .tracker import CentroidTracker, TrackResult

MODE_BRIGHT = "bright"
MODE_FACE = "face"


@dataclass
class FrameResult:
    detection: Detection | None
    track: TrackResult
    gray: np.ndarray
    mask: np.ndarray
    backend: str
    mode: str = MODE_BRIGHT


class DetectTrackPipeline:
    def __init__(
        self,
        threshold: int = 180,
        min_pixels: int = 12,
        gate_size: int = 80,
        max_lost: int = 15,
        mode: str = MODE_BRIGHT,
    ):
        self.detector = BrightSpotDetector(threshold=threshold, min_pixels=min_pixels)
        self.face_detector = FaceDetector()
        self.tracker = CentroidTracker(gate_size=gate_size, max_lost=max_lost)
        self.mode = MODE_FACE if mode == MODE_FACE else MODE_BRIGHT
        self._info = parallel.backend_info()

    @property
    def backend(self) -> str:
        return self._info["backend"]

    def set_mode(self, mode: str) -> str:
        self.mode = MODE_FACE if mode == MODE_FACE else MODE_BRIGHT
        self.tracker.reset()
        return self.mode

    def _detect(self, bgr: np.ndarray):
        if self.mode == MODE_FACE:
            return self.face_detector.detect(bgr)
        return self.detector.detect(bgr)

    def process(self, bgr: np.ndarray) -> FrameResult:
        h, w = bgr.shape[:2]
        det, gray, mask = self._detect(bgr)

        meas = None
        if det is not None:
            if not self.tracker.locked:
                meas = (det.x, det.y)
            else:
                # 锁定后只在波门内做局部检测，降低算量
                x0, y0, x1, y1 = self.tracker.gate_roi(h, w)
                roi = bgr[y0:y1, x0:x1]
                if roi.size > 0:
                    local, _, _ = self._detect(roi)
                    if local is not None:
                        meas = (local.x + x0, local.y + y0)

        track = self.tracker.step(meas, w, h)
        return FrameResult(
            detection=det,
            track=track,
            gray=gray,
            mask=mask,
            backend=self.backend,
            mode=self.mode,
        )

    def draw(self, bgr: np.ndarray, result: FrameResult) -> np.ndarray:
        vis = bgr.copy()
        h, w = vis.shape[:2]
        cx, cy = w // 2, h // 2
        cv2.drawMarker(vis, (cx, cy), (0, 255, 255), cv2.MARKER_CROSS, 24, 1)

        tr = result.track
        if tr.locked:
            x0, y0, x1, y1 = self.tracker.gate_roi(h, w)
            cv2.rectangle(vis, (x0, y0), (x1, y1), (0, 200, 0), 1)
            pt = (int(tr.x), int(tr.y))
            cv2.circle(vis, pt, 6, (0, 0, 255), 2)
            cv2.line(vis, (cx, cy), pt, (0, 128, 255), 1)
            text = f"LOCK miss=({tr.miss_x:+.1f},{tr.miss_y:+.1f}) lost={tr.lost_frames}"
            color = (0, 255, 0)
        else:
            text = "SEARCH"
            color = (0, 165, 255)
            if result.detection is not None:
                cv2.circle(
                    vis,
                    (int(result.detection.x), int(result.detection.y)),
                    5,
                    (255, 0, 0),
                    2,
                )

        cv2.putText(vis, text, (12, 28), cv2.FONT_HERSHEY_SIMPLEX, 0.7, color, 2)
        mode_tag = "face" if result.mode == MODE_FACE else f"thr={self.detector.threshold}"
        cv2.putText(
            vis,
            f"backend={result.backend} {mode_tag}",
            (12, 56),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.55,
            (220, 220, 220),
            1,
        )
        return vis
