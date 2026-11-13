"""人脸检测：OpenCV FaceDetectorYN (YuNet)，输出脸框中心作为跟踪测量。"""

from __future__ import annotations

from pathlib import Path

import cv2
import numpy as np

from .detector import Detection

_MODEL_CANDIDATES = [
    Path(__file__).resolve().parents[1] / "models" / "face_detection_yunet_2023mar.onnx",
    Path(__file__).resolve().parent / "face_detection_yunet_2023mar.onnx",
]


def _find_model() -> Path:
    for p in _MODEL_CANDIDATES:
        if p.is_file():
            return p
    raise FileNotFoundError(
        "YuNet model not found. Expected: models/face_detection_yunet_2023mar.onnx"
    )


class FaceDetector:
    def __init__(self, score_threshold: float = 0.7, nms_threshold: float = 0.3):
        self.score_threshold = float(score_threshold)
        self.nms_threshold = float(nms_threshold)
        self._model_path = str(_find_model())
        self._detector = None
        self._input_size = (0, 0)

    def _ensure(self, w: int, h: int) -> None:
        size = (w, h)
        if self._detector is not None and self._input_size == size:
            return
        self._detector = cv2.FaceDetectorYN_create(
            self._model_path,
            "",
            size,
            self.score_threshold,
            self.nms_threshold,
            5000,
        )
        self._input_size = size

    def detect(self, bgr: np.ndarray) -> tuple[Detection | None, np.ndarray, np.ndarray]:
        h, w = bgr.shape[:2]
        gray = cv2.cvtColor(bgr, cv2.COLOR_BGR2GRAY)
        mask = np.zeros((h, w), dtype=np.uint8)

        # YuNet needs reasonable size; skip tiny gate ROIs
        if min(h, w) < 48:
            return None, gray, mask

        self._ensure(w, h)
        assert self._detector is not None
        self._detector.setInputSize((w, h))
        _, faces = self._detector.detect(bgr)

        if faces is None or len(faces) == 0:
            return None, gray, mask

        # faces: [x, y, w, h, score, ...]
        best = max(faces, key=lambda f: float(f[2]) * float(f[3]))
        x, y, fw, fh = [float(v) for v in best[:4]]
        score = float(best[4]) if len(best) > 4 else 0.0
        x0, y0 = int(max(0, x)), int(max(0, y))
        x1, y1 = int(min(w, x + fw)), int(min(h, y + fh))
        if x1 <= x0 or y1 <= y0:
            return None, gray, mask

        mask[y0:y1, x0:x1] = 255
        cx = x + fw * 0.5
        cy = y + fh * 0.5
        area = int(fw * fh)
        return Detection(x=cx, y=cy, area=area, conf=score), gray, mask
