# imagedetect — 检测 / 跟踪 + 并行计算

在本机实现一套最小可用的光电风格流水线：

1. **并行预处理**（灰度 / 模糊 / 阈值）
2. **亮斑检测**（阈值 + 质心）
3. **质心跟踪**（波门 + 卡尔曼，输出脱靶量）

架构：**Qt 5.14.2 C++** 负责界面与流程编排，**Python** 只做算法（`src/` + `bridge/algo_api.py`）。

## 环境

```bash
cd D:\chengmao\imagedetect
pip install -r requirements.txt
```

当前机器若无 NVIDIA GPU，会自动走 **Numba CPU 多核并行**；检测到 CUDA 时灰度/阈值会切到 **Numba CUDA**。

### Qt 混编依赖

| 组件 | 本机示例路径 |
|------|----------------|
| Qt 5.14.2 | `D:\Qt\Qt5.14.2\5.14.2\msvc2017_64` |
| Python 开发包 | `C:\Users\rkx\AppData\Local\Python\pythoncore-3.14-64`（含 `Include`、`libs\python314.lib`、`python314.dll`） |
| MSVC | 与 Qt 套件匹配的 Visual Studio（本机可用 VS 生成工具） |

可选：设置环境变量 `PYTHON_ROOT` 指向 Python 安装根目录。

**C++ OpenCV 4.10** 已安装于 `D:\opencv\opencv\build`（含 `opencv_world4100`）。`build.bat` 会自动链接并把 DLL 拷到 `app\release`。未找到时摄像头/视频仍走 Python `cv2` shim。

## 运行 — Python CLI

```bash
python main.py                  # 合成亮斑动画
python main.py --camera 0       # 摄像头
python main.py --video your.mp4 # 视频文件
python main.py --bench          # 并行预处理耗时对比
```

| 键 | 作用 |
|----|------|
| `q` / `Esc` | 退出 |
| `r` | 复位跟踪 |
| `+` / `-` | 调整检测阈值 |

## 运行 — Qt 界面

一键编译（已写好本机 Qt 5.14.2 + MSVC 路径）：

```bash
cd D:\chengmao\imagedetect\app
build.bat
```

### Visual Studio 解决方案（.sln）

```bash
cd D:\chengmao\imagedetect\app
gen_sln.bat          # 从 .pro 生成/刷新 imagedetect.sln + .vcxproj
```

用 VS 打开：`D:\chengmao\imagedetect\app\imagedetect.sln`  
配置选 **Release | x64**（或 Debug | x64）。工具集为 **v145**（VS 2026）。

运行前把 Python / OpenCV / Qt 的 bin 加到 PATH，或先用 `build.bat` 把 `opencv_world4100.dll` 拷到输出目录。

或手动在 **x64 Native Tools** 终端：

```bash
cd D:\chengmao\imagedetect\app
qmake imagedetect.pro
nmake
```

运行前保证：

1. `python314.dll` 在 `PATH` 中（或与 exe 同目录）
2. 能 import 到仓库根下的 `bridge` / `src`（exe 在 `app\release` 时会自动向上探测）
3. 已 `pip install -r requirements.txt`

```bash
set PATH=C:\Users\rkx\AppData\Local\Python\pythoncore-3.14-64;%PATH%
D:\chengmao\imagedetect\app\release\imagedetect.exe
```

界面含两个页签：

- **产线运行**：PLC 连接/环回/自检、启动产线、模拟一发、结果表与追溯日志
- **算法沙盒**：原亮斑/人脸跟踪 demo（左结果 / 右掩膜）

## 一键运行

```bat
setup_runtime.bat
run.bat
```

会准备 Snap7/昂纳桩 DLL、海康 MVS 运行库、Python/OpenCV，并启动界面。

## 文档

讲解与数据规范见 [`doc/`](doc/README.md)：

- 系统架构、工位工艺  
- **雷管数据记录**字段与样例（`logs/results.csv`）  
- PLC 点位与联调  

## 雷管产线架构（已落地骨架）

```
PLC(S7/Mock) 硬触发相机 + 软触发点位
  → CaptureDispatcher (海康 ST1/ST2 + 昂纳 ST3，无 SDK 时走仿真帧)
  → AlgoDispatcher / bridge.algo_api (measure_od / measure_length / inspect_defect)
  → ProductTracker 槽位对号 → FinalJudge 配方判定
  → 写回 PassFail/FailCode → NG 入废料盒（由 PLC 执行）
  → logs/results.csv 追溯
```

配置：`config/plc_points.json`、`config/recipes/`、`config/calib/`。  
无 Snap7 时 `S7Client` 自动走内存 Mock，可先联调软件闭环。设置环境变量 `SNAP7_DIR` 后重新 qmake 即可接真机。

## 目录

```
imagedetect/
  config/                 # 点位表 / 配方 / 标定
  logs/                   # 运行追溯 CSV / NG 图
  bridge/algo_api.py      # C++ 嵌入 API（含测量入口）
  src/
    measure_od.py / measure_length.py / inspect_defect.py
    parallel.py / detector.py / tracker.py / pipeline.py
  app/
    core/                 # ProductRecord / Tracker / Judge / Recipe
    plc/                  # IPlcClient / S7 / Modbus / Mock / 点位表
    camera/               # ICamera / Hik / Onna / CaptureDispatcher
    algo/                 # AlgoDispatcher
    line/                 # LineController / ResultTrace
    MainWindow.*          # 产线 HMI + 沙盒
```

## 说明

- 产线主路径是状态机 + 多源采集；沙盒页保留原跟踪 demo。
- 海康 MVS / 昂纳 SDK / Snap7 通过宏接入（`USE_HIK_MVS` / `USE_SNAP7`），未链接时仿真可跑通对号与终判。
- Qt 侧不依赖 PyQt；摄像头/视频默认经 `bridge.algo_api.grab_*` 使用已安装的 `opencv-python`。
