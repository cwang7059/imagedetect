# SDK 接入说明

本工程对 PLC / 相机采用 **编译期探测 + 运行时动态加载**，缺 DLL 时自动回退 Mock/仿真，保证无硬件也能跑通软件闭环。

## 1. 海康 MVS（本机已安装）

| 项 | 路径 |
|----|------|
| 头文件 | `C:\Program Files (x86)\MVS\Development\Includes` |
| 库 | `...\Libraries\win64\MvCameraControl.lib` |
| 运行时 | `C:\Program Files (x86)\Common Files\MVS\Runtime\Win64_x64\MvCameraControl.dll` |

`imagedetect.pro` 检测到上述路径后自动定义 `USE_HIK_MVS` 并链接。

- 打开相机：枚举 GigE/USB/CameraLink，可按 `config/cameras.json` 的 index/serial 选择  
- 触发：`TriggerMode=ON`，`TriggerSource=Line0`（PLC 硬触发）  
- 软通知：`onHardTrigger` → `MV_CC_GetImageForBGR` 取帧  

## 2. Snap7（西门子 S7）

不强制链接 `.lib`。`Snap7Loader` 运行时 `LoadLibrary(snap7.dll)`，导出：

`Cli_Create / Cli_ConnectTo / Cli_DBRead / Cli_DBWrite / Cli_Disconnect / Cli_Destroy`

放置位置（任选）：

- `third_party/snap7/win64/snap7.dll`  
- 或与 `imagedetect.exe` 同目录  
- 或环境变量 `SNAP7_DIR`

获取：

```powershell
powershell -ExecutionPolicy Bypass -File third_party\fetch_sdks.ps1
```

无 DLL 时 `S7Client` 自动走内存 Mock。

## 3. Modbus TCP

`ModbusTcpClient` 使用 Qt `QTcpSocket` 原生实现（功能码 0x03 / 0x10），**无需 libmodbus**。

约定：unit id = DB 号低 8 位；保持寄存器地址 = 字节偏移 / 2（大端）。

## 4. 昂纳线扫（Onna）

厂商 SDK 不公开分发。工程定义稳定 C ABI：

`third_party/onna/include/onna_camera_c_api.h`

请厂商或集成方封装为 `OnnaCamera.dll`，放到：

- `third_party/onna/bin/OnnaCamera.dll`，或  
- 环境变量 `ONNA_SDK_DIR`

`OnnaLineScanCamera` 经 `OnnaLoader` 动态加载；找不到 DLL 时用仿真帧。

## 5. 相机配置

编辑 `config/cameras.json`：

```json
{
  "hik_od": { "index": 0, "serial": "" },
  "hik_len": { "index": 1, "serial": "" },
  "onna_defect": { "index": 0, "serial": "" }
}
```

## 6. 一键就绪（推荐）

在仓库根目录：

```bat
setup_runtime.bat   REM 编译 Snap7/昂纳桩 DLL + 拷贝海康/Python/OpenCV 运行库
run.bat             REM 设好 PYTHONHOME/PATH 后启动
```

说明：

- `third_party/snap7/win64/snap7.dll`、`third_party/onna/bin/OnnaCamera.dll` 为 **可运行桩库**（内存 DB / 仿真线扫），无真机时换成官方 DLL 即可。  
- 海康无相机时自动回退仿真帧，仍可「模拟一发」。  
- 启动后产线页：连接 PLC → 模拟一发 → 看结果表与 `logs/results.csv`。
