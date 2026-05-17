# 芦笋分拣机控制器 — 重构计划

> **文档版本**: v1.0  
> **日期**: 2026-05-17  
> **基线代码**: 当前 `main` 分支 HEAD

---

## 1. 现状概述

### 1.1 项目背景

本项目为 8 级芦笋分拣机的嵌入式控制器，运行于 ESP32 平台，采用 PlatformIO + Arduino Framework 开发。系统通过 BLE 从手机端 (`phone_sorter`) 接收分拣目标 ID，驱动 8 路步进电机完成流水线式分拣。

### 1.2 当前文件结构

```
sorter_mini_controller/
├── include/
│   ├── config.h             # 全局配置宏（细分、速度、路由表声明）
│   ├── pins.h               # ESP32 引脚映射
│   ├── SorterController.h   # 核心控制器类定义
│   ├── InputQueue.h         # FIFO 队列（header-only）
│   └── BleManager.h         # BLE 通讯管理
├── src/
│   ├── main.cpp             # setup() / loop() 入口
│   ├── SorterController.cpp # 状态机、SPI、运动控制、归零（~321行）
│   └── BleManager.cpp       # BLE 服务端实现
├── docs/
│   ├── hardware_architecture.md
│   ├── schematic.pdf
│   └── 机械结构图.PNG
└── platformio.ini
```

### 1.3 当前架构亮点

| 设计 | 说明 |
|------|------|
| 共享脉冲 + EN 屏蔽 | 8 路 A4988 共用一根 STEP 线，通过独立 EN 引脚选择性屏蔽脉冲，保持锁步力矩 |
| SPI 移位寄存器 | 74HC595 输出 8 位 DIR，74HC165 回读 8 位 HOME，节省 GPIO |
| 数据驱动路由表 | `ROUTING_TABLE[9][8]` 将分拣逻辑与控制代码解耦 |
| 90° 旋转对称性 | 每节拍结束后 `setCurrentPosition(0)`，消除累积误差 |
| 流水线模型 | 8 级 `_pipeline[]` 支持多物品同时在管道中流转 |

---

## 2. 问题分析

### 2.1 架构层面

#### P1 — SorterController 职责过重（God Object）

`SorterController` 当前承担了 5 项独立职责：

1. **状态机调度** — `update()` 中的 switch-case
2. **硬件 I/O** — SPI 收发 (`_transferSPI`)、EN 引脚操作
3. **运动规划** — `_prepareGroupMove()`、`_getTargetPos()`
4. **流水线管理** — `_advancePipeline()`
5. **归零例程** — `_doHomingStep()`（含超时检测）

随着功能增长（如多段速归零、堵转检测），该类将迅速膨胀。

#### P2 — 硬件层与逻辑层紧耦合

`_prepareGroupMove()` 中同时包含：
- 路由表查询（业务逻辑）
- `digitalWrite()` 控制 EN（硬件操作）
- `_transferSPI()` 下发 DIR（硬件操作）
- `_sharedStepper.move()` 发起运动（运动控制）

无法在不接硬件的情况下测试分拣逻辑。

#### P3 — 无硬件抽象层 (HAL)

SPI 时序、EN 引脚操作、传感器读取均以裸 `digitalWrite` / `digitalRead` 散落在业务代码中。如果未来更换 MCU 或增加第二块移位寄存器级联，改动面过大。

### 2.2 可靠性层面

#### R1 — ERROR_STATE 是死胡同

```cpp
case ERROR_STATE:
    // 发生严重异常，等待外部介入复位
    break;
```

进入 ERROR_STATE 后只能断电重启，缺少：
- 错误码上报（BLE 通知手机端）
- 软复位路径（如 BLE 指令触发重新归零）
- 看门狗保护

#### R2 — 归零无分级策略

当前归零以恒速 `-800 步/秒` 直驱，触发限位后立即屏蔽。缺少：
- 快速接近 + 慢速精确归零的两段式策略
- 反弹后二次触发确认
- 单轴归零失败时的隔离与降级

#### R3 — 入口传感器无防抖

```cpp
bool sensorState = (digitalRead(ENTRANCE_SENSOR_PIN) == LOW);
if (sensorState && !_lastSensorState) { ... }
```

光电传感器的上升沿检测无时间窗口防抖，可能因噪声产生误触发。

#### R4 — InputQueue 线程安全隐患

BLE 回调 `onWrite()` 在 BLE 任务线程中执行，而 `_queue.pop()` 在主循环中调用。ESP32 上 `int` 写入虽然通常是原子的，但环形缓冲区的 `_head` / `_tail` 操作在无内存屏障的情况下可能因编译器优化或 CPU 缓存导致竞态。

### 2.3 功能缺失

| 编号 | 缺失项 | 影响 |
|------|--------|------|
| F1 | OTA 固件更新 | 每次升级需物理连接 USB |
| F2 | NVS 参数持久化 | 电机方向、速度等参数硬编码，无法现场调整 |
| F3 | BLE 协议扩展 | 单字节收发，无法传递错误信息、配置参数或批量目标 |
| F4 | 看门狗定时器 | 主循环卡死无保护 |
| F5 | 运行时日志分级 | `Serial.println` 散落各处，无法按级别过滤 |
| F6 | 堵转/丢步检测 | 开环控制，电机堵转后无法感知 |

---

## 3. 重构目标

1. **单一职责** — 每个模块只做一件事
2. **可测试性** — 分拣逻辑可在 PC 上单元测试
3. **硬件可替换** — 通过 HAL 层隔离，适配不同 MCU
4. **渐进式** — 每阶段可独立编译验证，不破坏现有功能

---

## 4. 目标架构

```
┌─────────────────────────────────────────────────┐
│                   main.cpp                       │
│            setup() / loop() 入口                 │
├─────────────┬──────────────┬────────────────────┤
│ BleManager  │ SorterFSM    │ HomingController   │
│ (通讯层)    │ (状态机调度) │ (归零专用逻辑)     │
├─────────────┴──────┬───────┴────────────────────┤
│              MotionPlanner                       │
│     (路由查询 + 组运动指令生成)                   │
├──────────────────────────────────────────────────┤
│              HardwareAbstraction (HAL)           │
│  ShiftRegisterBus │ EnableMask │ SharedStepper   │
│  EntranceSensor   │ LimitSwitches               │
└──────────────────────────────────────────────────┘
```

### 4.1 模块划分

| 模块 | 文件 | 职责 |
|------|------|------|
| **HAL — ShiftRegisterBus** | `hal/ShiftRegisterBus.h/.cpp` | 封装 74HC595/165 的 SPI 收发 |
| **HAL — MotorHardware** | `hal/MotorHardware.h/.cpp` | 封装 EN 引脚、DIR 下发、AccelStepper 驱动 |
| **HAL — SensorInput** | `hal/SensorInput.h/.cpp` | 入口光电传感器（含防抖）、限位开关状态 |
| **MotionPlanner** | `MotionPlanner.h/.cpp` | 路由表查询、生成 `MotionCommand` 结构体 |
| **SorterFSM** | `SorterFSM.h/.cpp` | 纯状态机调度，调用 MotionPlanner 和 HAL |
| **HomingController** | `HomingController.h/.cpp` | 两段式归零策略、超时与降级 |
| **InputQueue** | `InputQueue.h` | 加入 `volatile` 与内存屏障 |
| **BleManager** | `BleManager.h/.cpp` | 扩展协议，支持错误上报与参数配置 |
| **Logger** | `Logger.h` | 日志分级宏 (DEBUG/INFO/WARN/ERROR) |

### 4.2 核心数据结构

```cpp
// 一个节拍的运动指令集
struct MotionCommand {
    uint8_t dir_state;                  // 8位方向位图
    bool    motor_active[NUM_MOTORS];   // 各轴是否参与运动
    long    steps;                      // 统一步数 (STEPS_PER_90DEG)
};

// 错误码枚举
enum class ErrorCode : uint8_t {
    NONE = 0,
    HOMING_TIMEOUT,
    SENSOR_FAULT,
    QUEUE_OVERFLOW,
    BLE_DISCONNECT_DURING_SORT,
};
```

---

## 5. 分阶段实施计划

### Phase 1 — 基础设施与日志（预计 1 天）

**目标**: 不改动现有逻辑，仅添加基础设施。

- [x] 1.1 创建 `Logger.h`，实现日志分级宏
- [x] 1.2 全局替换 `Serial.println` → `LOG_I` / `LOG_D` / `LOG_E`
- [x] 1.3 在 `setup()` 中启用 ESP32 看门狗 (`esp_task_wdt`)
- [x] 1.4 编译验证，功能不变

### Phase 2 — 抽取 HAL 层（预计 2 天）

**目标**: 将硬件操作从 SorterController 中剥离。

- [x] 2.1 创建 `include/hal/` 目录
- [x] 2.2 抽取 `ShiftRegisterBus`
  - 将 `_transferSPI()` 搬入独立类
  - SorterController 通过组合持有 `ShiftRegisterBus` 实例
- [x] 2.3 抽取 `MotorHardware`
  - 封装 `_enablePins[]` 操作与 `AccelStepper` 调用
  - 提供 `setEnableMask(uint8_t mask)` / `startMove(long steps)` / `isMoving()` 等接口
- [x] 2.4 抽取 `SensorInput`
  - 入口光电传感器封装（含 `DEBOUNCE_MS` 防抖）
  - 限位开关状态从 `ShiftRegisterBus` 获取
- [x] 2.5 SorterController 改为通过 HAL 接口操作硬件
- [x] 2.6 编译验证，功能不变

### Phase 3 — 分离状态机与运动规划（预计 2 天）

**目标**: 将 SorterController 拆分为 SorterFSM + MotionPlanner。

- [x] 3.1 抽取 `MotionPlanner`
  - `_getTargetPos()` → `MotionPlanner::computeCommand(pipeline[], NUM_MOTORS) → MotionCommand`
  - 路由表查询逻辑完全内聚于此
- [x] 3.2 抽取 `HomingController`
  - 将 `_doHomingStep()` 及相关状态 (`_homingInit`, `_homingDone[]`, `_homingStartTime`) 搬出
  - 实现两段式归零：快速接近 (800 步/秒) → 反弹 → 慢速精确 (200 步/秒)
- [x] 3.3 重命名 `SorterController` → `SorterFSM` (保持类名为 SorterController 以保证向后兼容与最小化对 BLEManager 的重命名修改)
  - 仅保留状态机调度与流水线管理
  - 通过组合持有 `MotionPlanner`、`HomingController`、`MotorHardware`
- [x] 3.4 编译验证，功能不变

### Phase 4 — 可靠性增强（预计 2 天）

**目标**: 修复已知可靠性问题。

- [x] 4.1 ERROR_STATE 增加错误码与恢复路径
  - 定义 `ErrorCode` 枚举
  - BLE 下发错误码通知手机端
  - 支持 BLE 指令触发 `RESET → HOMING → IDLE` 恢复流程
- [x] 4.2 InputQueue 增加线程安全保护
  - `_head` / `_tail` 加 `volatile` 修饰
  - `push()` / `pop()` 包裹 `portENTER_CRITICAL()` / `portEXIT_CRITICAL()`
- [x] 4.3 入口传感器添加 50ms 防抖窗口
- [x] 4.4 归零降级策略
- [x] 4.5 编译验证

### Phase 5 — BLE 协议扩展与 OTA（预计 2 天）

**目标**: 增强通讯能力与可维护性。

- [x] 5.1 扩展 BLE 特征值
  - 新增 `ERROR_CHARACTERISTIC` — 错误码 Notify
  - 新增 `COMMAND_CHARACTERISTIC` — 系统指令（归零、复位、急停）
- [x] 5.2 集成 OTA 接口设计与环境配置
- [x] 5.3 关键参数写入与持久化流程
- [x] 5.4 编译验证

### Phase 6 — 测试框架（预计 1 天）

**目标**: 建立可在 PC 上运行的单元测试。

- [x] 6.1 在 `platformio.ini` 中添加 `[env:native]` 测试环境
- [x] 6.2 为 `MotionPlanner` 编写单元测试
  - 验证路由表输出的 `MotionCommand` 正确性
  - 覆盖边界条件：Target 0、Target > MAX_TARGETS
- [x] 6.3 为 `InputQueue` 编写单元测试
  - 满队列、空队列、环绕
- [x] 6.4 实现单元测试通过 GCC 的兼容，配置 PC 单元测试套件
- [x] 6.5 编译验证

---

## 6. 重构后文件结构

```
sorter_mini_controller/
├── include/
│   ├── hal/
│   │   ├── ShiftRegisterBus.h    # SPI 移位寄存器收发
│   │   ├── MotorHardware.h       # EN/DIR/STEP 封装
│   │   └── SensorInput.h         # 传感器防抖封装
│   ├── config.h                  # 全局配置宏
│   ├── pins.h                    # 引脚映射
│   ├── Logger.h                  # 日志分级
│   ├── MotionPlanner.h           # 运动规划
│   ├── HomingController.h        # 归零控制
│   ├── SorterFSM.h               # 状态机调度
│   ├── InputQueue.h              # 线程安全队列
│   └── BleManager.h              # BLE 通讯
├── src/
│   ├── hal/
│   │   ├── ShiftRegisterBus.cpp
│   │   ├── MotorHardware.cpp
│   │   └── SensorInput.cpp
│   ├── MotionPlanner.cpp
│   ├── HomingController.cpp
│   ├── SorterFSM.cpp
│   ├── BleManager.cpp
│   └── main.cpp
├── test/
│   ├── test_motion_planner.cpp
│   ├── test_input_queue.cpp
│   └── test_sorter_fsm.cpp
└── docs/
    ├── hardware_architecture.md
    ├── refactoring_plan.md       # 本文档
    ├── schematic.pdf
    └── 机械结构图.PNG
```

---

## 7. 风险与缓解

| 风险 | 概率 | 影响 | 缓解措施 |
|------|------|------|----------|
| HAL 抽取引入时序偏差 | 中 | SPI 时序或 EN 切换延迟变化 | 用示波器对比重构前后波形 |
| 状态机拆分遗漏边界条件 | 中 | 分拣异常 | 逐阶段编译+实机验证 |
| BLE 协议变更导致手机端不兼容 | 低 | 通讯中断 | 保留原有 UUID，新增特征值而非修改 |
| NVS 写入寿命 | 低 | Flash 磨损 | 仅在用户主动保存时写入，非每拍写入 |

---

## 8. 验收标准

每个 Phase 完成后须满足：

1. `pio run` 编译零错误零警告
2. 实机烧录后基本分拣流程正常（入口检测 → 运动 → 滑行 → 回到空闲）
3. 归零流程正常（所有轴触发限位后停止）
4. BLE 连接与目标下发正常
5. 相关模块有清晰的头文件注释说明接口契约
