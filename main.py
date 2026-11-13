"""
亮斑检测 + 质心跟踪 Demo

用法:
  python main.py                  # 合成目标动画（无需摄像头）
  python main.py --camera 0       # 摄像头
  python main.py --video path.mp4 # 视频文件
  python main.py --bench          # 并行预处理耗时对比
"""

from __future__ import annotations

import argparse
import time
from pathlib import Path

import cv2
import numpy as np

from src.parallel import backend_info, blur3, threshold, to_gray
from src.pipeline import DetectTrackPipeline


def make_synth_frame(t: float, w: int = 640, h: int = 480) -> np.ndarray:
    """生成带移动亮斑的合成帧，模拟光电目标。"""
    img = np.zeros((h, w, 3), dtype=np.uint8)
    img[:] = (18, 18, 22)
    # 背景噪声
    noise = np.random.randint(0, 25, (h, w, 3), dtype=np.uint8)
    img = cv2.add(img, noise)

    cx = int(w * 0.5 + 180 * np.sin(t * 1.3))
    cy = int(h * 0.5 + 110 * np.cos(t * 0.9))
    cv2.circle(img, (cx, cy), 7, (255, 255, 255), -1)
    cv2.circle(img, (cx, cy), 14, (80, 80, 80), 1)
    return img


def run_stream(source: str | int | None, thr: int, gate: int) -> None:
    pipe = DetectTrackPipeline(threshold=thr, gate_size=gate)
    info = backend_info()
    print(f"并行后端: {info}")

    cap = None
    if source is None:
        mode = "synth"
    elif isinstance(source, int) or str(source).isdigit():
        mode = "camera"
        cap = cv2.VideoCapture(int(source))
    else:
        mode = "video"
        cap = cv2.VideoCapture(str(source))
        if not cap.isOpened():
            raise SystemExit(f"无法打开视频: {source}")

    t0 = time.perf_counter()
    frame_id = 0
    fps_ema = 0.0

    while True:
        if mode == "synth":
            t = time.perf_counter() - t0
            frame = make_synth_frame(t)
        else:
            ok, frame = cap.read()
            if not ok:
                if mode == "video":
                    cap.set(cv2.CAP_PROP_POS_FRAMES, 0)
                    continue
                break

        t1 = time.perf_counter()
        result = pipe.process(frame)
        dt = time.perf_counter() - t1
        fps = 1.0 / max(dt, 1e-6)
        fps_ema = fps if frame_id == 0 else fps_ema * 0.9 + fps * 0.1

        vis = pipe.draw(frame, result)
        cv2.putText(
            vis,
            f"FPS {fps_ema:.1f}",
            (12, 84),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.55,
            (200, 255, 200),
            1,
        )

        # 右侧显示掩膜
        mask_bgr = cv2.cvtColor(result.mask, cv2.COLOR_GRAY2BGR)
        panel = np.hstack([vis, mask_bgr])
        cv2.imshow("imagedetect - detect/track", panel)

        key = cv2.waitKey(1) & 0xFF
        if key == ord("q") or key == 27:
            break
        if key == ord("r"):
            pipe.tracker.reset()
            print("跟踪已复位")
        if key in (ord("+"), ord("=")):
            pipe.detector.threshold = min(255, pipe.detector.threshold + 5)
        if key == ord("-"):
            pipe.detector.threshold = max(0, pipe.detector.threshold - 5)

        frame_id += 1

    if cap is not None:
        cap.release()
    cv2.destroyAllWindows()


def run_bench(loops: int = 30) -> None:
    info = backend_info()
    print(f"后端: {info}")
    img = make_synth_frame(0.5, 1280, 720)
    # warmup
    for _ in range(3):
        g = to_gray(img)
        b = blur3(g)
        threshold(b, 180)

    t0 = time.perf_counter()
    for _ in range(loops):
        g = to_gray(img)
        b = blur3(g)
        threshold(b, 180)
    dt = time.perf_counter() - t0
    print(f"1280x720 预处理 x{loops}: {dt*1000:.1f} ms, 单帧 {dt/loops*1000:.2f} ms")

    # OpenCV 对照
    t0 = time.perf_counter()
    for _ in range(loops):
        g = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
        b = cv2.blur(g, (3, 3))
        _, _m = cv2.threshold(b, 180, 255, cv2.THRESH_BINARY)
    dt2 = time.perf_counter() - t0
    print(f"OpenCV 对照 x{loops}: {dt2*1000:.1f} ms, 单帧 {dt2/loops*1000:.2f} ms")


def main() -> None:
    parser = argparse.ArgumentParser(description="图像检测/跟踪 + 并行计算 Demo")
    parser.add_argument("--camera", type=int, default=None, help="摄像头索引")
    parser.add_argument("--video", type=str, default=None, help="视频路径")
    parser.add_argument("--threshold", type=int, default=180, help="亮斑阈值")
    parser.add_argument("--gate", type=int, default=80, help="跟踪波门边长")
    parser.add_argument("--bench", action="store_true", help="跑并行预处理基准测试")
    args = parser.parse_args()

    if args.bench:
        run_bench()
        return

    source: str | int | None
    if args.camera is not None:
        source = args.camera
    elif args.video:
        source = args.video
        if not Path(args.video).exists():
            raise SystemExit(f"文件不存在: {args.video}")
    else:
        source = None

    run_stream(source, thr=args.threshold, gate=args.gate)


if __name__ == "__main__":
    main()
