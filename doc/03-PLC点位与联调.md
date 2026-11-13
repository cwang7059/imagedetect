# PLC 点位与联调说明

点位权威配置：`config/plc_points.json`（DB100，大端）。

## 1. 通信

| 项 | 默认 |
|----|------|
| 协议 | S7（无 Snap7 时上位机用内存 Mock） |
| IP / Rack / Slot | `192.168.0.1` / 0 / 1 |
| 轮询 | 20 ms |
| 心跳 | PC 写 `PcAlive` 约 500 ms 翻转 |

## 2. PLC → PC（触发与对号）

| 点位 | 含义 |
|------|------|
| `Trig_ST1/2/3` | 对应工位软触发边沿 |
| `Trig_Judge` | 终判工位请求结果 |
| `Slot_ST*` / `Slot_Judge` | 当前雷管槽位 |
| `ProductSeq` | 流水号 |
| `RecipeId` | 换型配方号 |
| `SysReady` / `Reset` | 系统就绪 / 清空槽位表 |

## 3. PC → PLC（结果与剔除）

| 点位 | 含义 |
|------|------|
| `Result_Ready` | 本槽结果已写出 |
| `Result_Slot` | 结果对应槽位 |
| `PassFail` | 1 合格 / 0 不合格 |
| `FailCode` | 见数据记录规范 |
| `Measure_D1/D2/L` | 尺寸回显（可选屏显） |
| `DefectGrade` | 瑕疵等级回显 |
| `Busy` / `Fault` | 上位机忙 / 故障 |

PLC 在剔除工位读 `PassFail`：合格放行，不合格驱动废料盒气缸。超时未收到结果应按 NG 处理（与上位机策略一致）。

## 4. 建议联调顺序

1. 仅 PLC：点位读写环回（HMI「环回测试」）  
2. 单工位硬触发 → 存图 → 人工量尺寸  
3. 三工位对号 + 终判写回（空跑）  
4. 样件连续跑，核对 `logs/results.csv`  
5. 换型配方与报警/自检收尾  

## 5. 真机接入

- 设置环境变量 `SNAP7_DIR` 后重新 `qmake` / `build.bat`  
- 海康 MVS：编译宏 `USE_HIK_MVS`  
- 昂纳 SDK：在 `OnnaLineScanCamera` 中替换仿真实现  
