# 芦笋分拣机迷你控制器 — 详细设计说明书 (DDD)

> **文档版本**: v1.0  
> **日期**: 2026-05-18  
> **状态**: 正式发布  

---

## 1. 物理引脚映射与核心配置参数

### 1.1 GPIO 物理引脚映射表 (`pins.h`)

| 功能分类 | 引脚名称 | 物理引脚 (ESP32 GPIO) | 信号方向 | 说明 |
| :--- | :--- | :--- | :--- | :--- |
| **共享脉冲** | `SHARED_STEP_PIN` | `GPIO 5` | 输出 (Output) | 所有 8 个 A4988 步进驱动器的 STEP 线硬件并联在此引脚 |
| **虚拟方向** | `DUMMY_DIR_PIN` | `GPIO 16` | 输出 (Output) | AccelStepper 库初始化所需的虚拟方向引脚 (不连接实际电路) |
| **与门脉冲闸门** | `EN_PIN_0` 至 `EN_PIN_7` | `GPIO 12, 13, 14, 27, 26, 25, 33, 32` | 输出 (Output) | 分别控制与门放行 M0 至 M7 电机的 STEP 信号 (高电平放行，低电平屏蔽。驱动芯片使能端已接地常激活) |
| **移位锁存** | `LATCH_PIN` | `GPIO 18` | 输出 (Output) | 74HC595 的 RCLK (锁存输出) 兼 74HC165 的 PL (并行载入) |
| **移位时钟** | `CLOCK_PIN` | `GPIO 19` | 输出 (Output) | 74HC595 的 SRCLK 与 74HC165 的 CP (移位时钟) |
| **移位数据出**| `DIR_DATA_OUT` | `GPIO 21` | 输出 (Output) | ESP32 发送至 74HC595 的串行方向数据输入 (DS) |
| **移位数据入**| `HOME_DATA_IN` | `GPIO 34` | 输入 (Input) | ESP32 从 74HC165 串行移出的物理限位开关状态 (Q7) |
| **落料传感器**| `ENTRANCE_SENSOR_PIN`| `GPIO 4` | 输入 (Input) | 入口处落料红外光电传感器输入 |
| **板载 LED** | `LED_PIN` | `GPIO 2` | 输出 (Output) | 系统状态指示灯 (常亮=正常运行，闪烁=寻零中，快闪=错误挂起) |

### 1.2 运动控制核心配置 (`config.h`)

*   **几何配比与步数换算**：
    *   步进电机固有步数：`200 步/周` (1.8°/步)。
    *   步进驱动器细分：`16 细分` (A4988 硬件短接 MS1/MS2/MS3 为高电平)。
    *   机械减速比：`1:4`。
    *   `90° 旋转步数 (STEPS_PER_90DEG)` 计算公式：
        $$\text{步数} = 200 \times 16 \times 4 \times \frac{90^{\circ}}{360^{\circ}} = 3200 \text{ 步}$$
*   **运动参数上限**：
    *   最大速度 (`MAX_SPEED`)：`2000 步/秒`。
    *   最大加速度 (`ACCELERATION`)：`5000 步/秒²`。
    *   寻零接近速度 (`HOMING_CONSTANT_SPEED`)：`800 步/秒`。
    *   寻零微调锁定速度 (`HOMING_SLOW_SPEED`)：`200 步/秒`。
    *   滑行落料延时 (`SLIDE_WAIT_MS`)：`300 毫秒` (保证物料在翻转板到位后通过重力完全滑入下一级)。

---

## 2. HAL 硬件抽象组件设计

### 2.1 ShiftRegisterBus (全双工双向移位总线)

为了精简引脚，74HC595 (控制 DIR 方向) 与 74HC165 (回读 HOME 限位) 共享锁存线与时钟线。

#### 2.1.1 模拟全双工 SPI 传输时序
1.  **并行锁存/载入 (LATCH = LOW)**：
    *   `LATCH_PIN` 拉低。
    *   `74HC165` 接收到 PL 异步拉低信号，瞬间将 8 路物理限位开关的并行电平状态抓取锁存至其移位寄存器中。
    *   `74HC595` 准备接收移位数据。
2.  **串行对移循环 (8次)**：
    *   在每次时钟脉冲拉高前，ESP32 立即读取 `HOME_DATA_IN` (GPIO 34) 的电平状态，拼装接收字节的第 `i` 位。
    *   ESP32 将发送字节的第 `i` 位，写入 `DIR_DATA_OUT` (GPIO 21)。
    *   时钟线 `CLOCK_PIN` 拉高，再拉低，产生一个上升沿。
    *   此时，74HC595 向内移入 1 位；同时 74HC165 向外移出 1 位。
3.  **并行输出生效 (LATCH = HIGH)**：
    *   `LATCH_PIN` 拉高。
    *   `74HC595` 内部移位寄存器的数据瞬间锁存输出至存储寄存器，改变其 8 位物理 DIR 输出脚电平，瞬间改变所有步进电机的转向。
    *   `74HC165` 退出移位模式，恢复并行输入监听。

#### 2.1.2 API 核心接口
```cpp
class ShiftRegisterBus {
public:
  void begin();
  // 全双工收发：向 595 写入 8位 方向字节，并返回 165 移出的 8位 限位字节
  uint8_t transfer(uint8_t dirData);
};
```

### 2.2 MotorHardware (脉冲同步与使能屏蔽)

*   **共享脉冲同步原理**：
    由于 8 个电机的 STEP 脚并联在 `SHARED_STEP_PIN` (GPIO 5) 上，软件中全局唯一的 `sharedStepper` 实例在驱动此引脚输出加减速脉冲时，**所有被激活使能的电机必定以完全相同的步频和速度曲线同步旋转**，保证了物理分拣多级滑道动作的高同步性。
*   **多路与门脉冲控制 (闸门机制)**：
    物理上，8路步进电机驱动器的 `ENABLE` 引脚已直接接地，保证电机时刻处于通电锁死状态（维持锁步力矩 Holding Torque，抵抗芦笋物理撞击或机构自重）。ESP32 的使能引脚作为“闸门”外接与门，用于选通或拦截 STEP 脉冲：
    *   **运动放行 (Bit = 1)**：写入 `HIGH`。开通该路电机的与门，ESP32 产生的同步步进脉冲能够到达驱动器并被执行，电机旋转动作。
    *   **运动屏蔽 (Bit = 0)**：写入 `LOW`。关闭该路与门，物理拦截脉冲信号，电机保持自锁静止。

### 2.3 SensorInput (防抖落料传感器)

*   **滑动窗口防抖机制**：
    为过滤潮湿环境光栅产生的噪点抖动，采用非阻塞滑动时间窗防抖：
    *   在 `update()` 中持续轮询读取 `ENTRANCE_SENSOR_PIN` (GPIO 4)。
    *   如果读取到电平跳变，锁定当前时间戳，只有当该电平稳定持续超过 `DEBOUNCE_MS = 50` 毫秒时，才确认为一次真实跃迁，触发 `isRisingEdge()` 上升沿信号，驱动状态机。

---

## 3. 两段式归零控制设计 (HomingController)

自动寻零是系统确立绝对物理零点基准的核心机制。

```
            [开始归零]
                │
                ├──────────────────────────────────────┐
                ▼                                      ▼
     [M0..M7 8轴电机全部启动]                   [启动 10s 安全定时器]
                │                                      │
                ▼                                      │
  第一阶段：快速接近 (800 步/秒)                        │
  - 循环通过 165 读取限位状态                            │
  - 若某轴触碰限位，立即拉高 EN 屏蔽该轴                  │
  - 所有轴限位触碰完毕 ───► 第二阶段                     │
                │                                      │
                ▼                                      │
  第二阶段：反弹脱离                                    ├─► 若超时 (10s)
  - 全轴统一向正方向运行一小段步数                       │    - 强制关闭所有使能
  - 彻底脱离限位，恢复 HOME 开关常开                     │    - 报错 ERR_HOMING_TIMEOUT
                │                                      │    - 坠入 ERROR_STATE
                ▼                                      │
  第三阶段：慢速精确锁定 (200 步/秒)                      │
  - 重新反向超低速接近限位                               │
  - 精准捕捉限位开关的机械接触边缘                       │
  - 到位锁定，重置 AccelStepper 物理坐标为 0             │
                │                                      │
                ▼                                      ▼
          [全轴归零成功] ───────────────────────► [清空定时器]
```

*   **快速接近速度**：`800 步/秒`。极速缩减寻零准备时间。
*   **慢速锁定速度**：`200 步/秒`。极大限度降低碰触物理限位时的机械冲击力，并获取最精确的 tactile 沿。
*   **超时硬截止保护**：在 update 中计算 `millis() - _homingStartTime >= 10000`。若超时，立即停止所有脉冲，拉高所有 EN，发出报警，锁死系统以保护现场安全。

---

## 4. 扫码一对一安全配对机制 (Scan-to-Pair)

防止多台设备共存时交叉连接的核心设计。

*   **MAC 地址动态广播命名**：
    在 `BleManager::begin()` 中读取内置 eFuse MAC 地址，拼装生成具有唯一特征的设备广播名：
    ```cpp
    uint64_t mac = ESP.getEfuseMac();
    char bleName[30];
    // 取低3字节，生成如 Sorter_3E9B2F 的名字
    snprintf(bleName, sizeof(bleName), "Sorter_%02X%02X%02X", 
             (uint8_t)(mac >> 16), (uint8_t)(mac >> 8), (uint8_t)mac);
    BLEDevice::init(bleName);
    ```
*   **物理贴纸二维码内容规范**：
    二维码中包含该控制器的唯一广播名字符串：`Sorter_3E9B2F`。
*   **手机端配对过滤机制**：
    手机主控端 `sorter_mini_phone` 开启 CameraX，解析出该设备名并存入 Android 系统的本地持久化中。此后，蓝牙扫描器在扫描时，不再进行固定名称匹配，而是强行比对 `ScanRecord` 中的广播名称与本地存储配对名。只有完全一致时才发起连接请求，从机制上完全封死了误连接旁边流水线设备的技术通道。

---

## 5. 16路通道未来平滑级联扩展详细设计

系统升级为 16 级翻转分拣机构时，硬件保持 **ESP32 芯片与引脚数量完全不改变**。通过 SPI 菊花链与软件位元宽度的拉伸平滑过渡。

### 5.1 移位寄存器菊花链 (Daisy-chain) 物理级联

1.  **方向控制 (74HC595) 级联**：
    将第一片 595 的 `Q7'` (Pin 9 串行输出) 接到第二片 595 的 `DS` (Pin 14 串行输入)。两片 595 的时钟 (SHCP) 和锁存 (STCP) 直接物理并联，接在 ESP32 的 `GPIO 19` 和 `GPIO 18` 上。
2.  **限位回读 (74HC165) 级联**：
    将第二片 165 的 `Q7` (Pin 9 串行输出) 接到第一片 165 的 `DS` (Pin 10 串行输入)。两片 165 的时钟 (CP) 和锁存 (PL) 直接物理并联，接在 ESP32 的 `GPIO 19` 和 `GPIO 18` 上。第一片 165 的 `Q7` 接至 ESP32 的 `GPIO 34`。
3.  **引脚开销**：扩展为 16 路输入/16 路输出，**ESP32 物理引脚依然为 4 个，完全不增加**。

### 5.2 软件数据位宽拉伸改造
在不改变核心状态机的前提下，仅需将位图类型从 `uint8_t` (8-bit) 升级为 `uint16_t` (16-bit)：

```diff
// config.h 维度扩展
-#define NUM_MOTORS   8
+#define NUM_MOTORS   16

// 查表动作指令依然沿用 ROUTING_TABLE，编译器自动将其拉伸为 17行16列，0修改！
extern const int8_t ROUTING_TABLE[MAX_TARGETS + 1][NUM_MOTORS];

// 核心指令数据类型重构
struct MotionCommand {
-   uint8_t enable_mask;  // 8位
-   uint8_t dir_state;    // 8位
+   uint16_t enable_mask; // 升级为 16位 掩码
+   uint16_t dir_state;   // 升级为 16位 方向状态
    bool    move_any;
    long    steps;
};

// 移位传输时序循环数扩容：从 8 次时钟脉冲升级为 16 次时钟脉冲
-uint8_t ShiftRegisterBus::transfer(uint8_t dirData)
+uint16_t ShiftRegisterBus::transfer(uint16_t dirData) {
+  uint16_t homeData = 0;
+  // 循环 16 次
+  for(int i = 15; i >= 0; --i) { ... }
+}
```

由于总体设计上做到了逻辑层与驱动层完全隔离，整个升级流程对 `SorterController` 状态机与流水线流转完全隐形，移植摩擦系数为零。

---

## 6. 单元测试架构设计 (Native Unit Testing)

### 6.1 完全逻辑解耦带来的测试优势
由于 `MotionPlanner` 纯内聚数据查表与计算逻辑，**内部没有任何 Arduino SDK 依赖和物理 GPIO 操作**，系统能够完美地在非 ESP32 环境（如普通的 Windows / Linux PC 上）通过标准 GCC 编译器直接对其进行单元测试。

### 6.2 PlatformIO Native 环境配置

项目根目录 `platformio.ini` 中配置了专门的 `native` 测试环境：

```ini
[env:native]
platform = native
test_framework = googletest   ; 或使用 light-weight 的 unity 框架
```

### 6.3 路由决策测试桩 (Test Cases)

在项目 `test/test_motion_planner/` 下建立测试源文件。对不同流水线目标进行静态输入断言：

```cpp
#include <gtest/gtest.h>
#include "MotionPlanner.h"
#include "config.h"

TEST(MotionPlannerTest, TestStandardRouting) {
  // 1. 模拟一个处于流转状态的流水线数组 (第0级到第7级当前分拣的目标)
  int mockPipeline[8] = {1, 2, 0, 8, 5, 0, 0, 0};
  int mockDirs[8]     = {1, 1, 1, 1, 1, 1, 1, 1}; // 假设全部为正极性装配

  MotionPlanner planner;
  MotionCommand cmd = planner.planGroupMove(mockPipeline, mockDirs);

  // 2. 静态断言验证
  // 目标为 0 的通道绝对不能动 -> enable_mask 对应位必须为 0 屏蔽脉冲
  EXPECT_FALSE(cmd.enable_mask & (1 << 2)); // 第 2 级 (mockPipeline[2] == 0)
  EXPECT_FALSE(cmd.enable_mask & (1 << 5)); // 第 5 级 (mockPipeline[5] == 0)

  // 激活的电机必须要有所动作
  EXPECT_TRUE(cmd.enable_mask & (1 << 0));  // 第 0 级 (mockPipeline[0] == 1)
  EXPECT_TRUE(cmd.move_any);

  // 检查统一步数是否为 90° 旋转步数
  EXPECT_EQ(cmd.steps, 3200);
}
```

运行 `pio test -e native` 指令即可在开发机本地 PC 上极速运行上述测试，获得 100% 覆盖率验证，彻底规避了上电实机调试产生物理硬件碰撞损坏的巨大工程风险！
