"""质心跟踪 + 简易卡尔曼平滑，输出脱靶量。"""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np


@dataclass
class TrackResult:
    x: float
    y: float
    miss_x: float  # 相对画面中心的脱靶量（像素）
    miss_y: float
    locked: bool
    lost_frames: int


class CentroidTracker:
    """
    检测建锁后，在局部波门内更新；连续丢检超过 max_lost 则解锁。
    状态: [x, y, vx, vy]
    """

    def __init__(
        self,
        gate_size: int = 80,
        max_lost: int = 15,
        process_var: float = 4.0,
        measure_var: float = 9.0,
    ):
        self.gate_size = gate_size
        self.max_lost = max_lost
        self.q = process_var
        self.r = measure_var
        self.reset()

    def reset(self) -> None:
        self.locked = False
        self.lost = 0
        self.state = np.zeros(4, dtype=np.float64)
        self.P = np.eye(4, dtype=np.float64) * 100.0

    def _predict(self) -> None:
        F = np.array(
            [
                [1, 0, 1, 0],
                [0, 1, 0, 1],
                [0, 0, 1, 0],
                [0, 0, 0, 1],
            ],
            dtype=np.float64,
        )
        Q = np.eye(4, dtype=np.float64) * self.q
        self.state = F @ self.state
        self.P = F @ self.P @ F.T + Q

    def _update(self, zx: float, zy: float) -> None:
        H = np.array([[1, 0, 0, 0], [0, 1, 0, 0]], dtype=np.float64)
        z = np.array([zx, zy], dtype=np.float64)
        R = np.eye(2, dtype=np.float64) * self.r
        y = z - H @ self.state
        S = H @ self.P @ H.T + R
        K = self.P @ H.T @ np.linalg.inv(S)
        self.state = self.state + K @ y
        self.P = (np.eye(4) - K @ H) @ self.P

    def gate_roi(self, frame_h: int, frame_w: int) -> tuple[int, int, int, int]:
        """返回 (x0, y0, x1, y1)。"""
        half = self.gate_size // 2
        cx, cy = int(self.state[0]), int(self.state[1])
        x0 = max(0, cx - half)
        y0 = max(0, cy - half)
        x1 = min(frame_w, cx + half)
        y1 = min(frame_h, cy + half)
        return x0, y0, x1, y1

    def step(
        self,
        meas_xy: tuple[float, float] | None,
        frame_w: int,
        frame_h: int,
    ) -> TrackResult:
        cx0 = (frame_w - 1) * 0.5
        cy0 = (frame_h - 1) * 0.5

        if not self.locked:
            if meas_xy is None:
                return TrackResult(0, 0, 0, 0, False, self.lost)
            self.state[:] = [meas_xy[0], meas_xy[1], 0.0, 0.0]
            self.P = np.eye(4) * 10.0
            self.locked = True
            self.lost = 0
            mx, my = meas_xy[0] - cx0, meas_xy[1] - cy0
            return TrackResult(meas_xy[0], meas_xy[1], mx, my, True, 0)

        self._predict()

        if meas_xy is not None:
            # 波门外测量视为无效，避免跳锁
            x0, y0, x1, y1 = self.gate_roi(frame_h, frame_w)
            if x0 <= meas_xy[0] <= x1 and y0 <= meas_xy[1] <= y1:
                self._update(meas_xy[0], meas_xy[1])
                self.lost = 0
            else:
                self.lost += 1
        else:
            self.lost += 1

        if self.lost > self.max_lost:
            self.reset()
            return TrackResult(0, 0, 0, 0, False, self.lost)

        x, y = float(self.state[0]), float(self.state[1])
        return TrackResult(x, y, x - cx0, y - cy0, True, self.lost)
