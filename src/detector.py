"""亮斑 / 目标检测：阈值分割 + 质心。"""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np

from . import parallel


@dataclass
class Detection:
    x: float
    y: float
    area: int
    conf: float


class BrightSpotDetector:
    """适合红外/激光类亮目标的简单检测器。"""

    def __init__(self, threshold: int = 180, min_pixels: int = 12):
        self.threshold = threshold
        self.min_pixels = min_pixels

    def detect(self, bgr: np.ndarray) -> tuple[Detection | None, np.ndarray, np.ndarray]:
        gray = parallel.to_gray(bgr)
        blurred = parallel.blur3(gray)
        mask = parallel.threshold(blurred, self.threshold)
        cx, cy, n = parallel.centroid_from_mask(mask, self.min_pixels)
        if n <= 0:
            return None, gray, mask
        # 面积越大、越亮，置信度略高（简单启发式）
        conf = min(1.0, n / 200.0)
        return Detection(x=cx, y=cy, area=int(n), conf=conf), gray, mask
