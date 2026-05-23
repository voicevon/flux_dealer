#ifndef CONFIG_H
#define CONFIG_H

#include <limits.h>

// ============================================================================
// 硬件配置：ESP32
// ============================================================================

// --- 细分配置（MS1/MS2/MS3 引脚已连至 MCU GPIO，由软件控制）---
// 板载驱动芯片：A4988 / HR4988（包含 MS1, MS2, MS3）
// A4988 真值表：
//   MS1=L MS2=L MS3=L -> 全步(1)     MS1=H MS2=L MS3=L -> 1/2步
//   MS1=L MS2=H MS3=L -> 1/4步     MS1=H MS2=H MS3=L -> 1/8步
//   MS1=H MS2=H MS3=H -> 1/16步
// 引脚定义见 pins.h（MS1_PIN / MS2_PIN / MS3_PIN）
#define MICROSTEP_RESOLUTION  1    // 软件目标细分：1 | 2 | 4 | 8 | 16

// --- 分拣逻辑配置 ---
#define MAX_TARGETS           8     // 最大支持的目标槽位数

#define NUM_MOTORS            8     // 电机数量

// 路由表：定义 Target ID (1~8) 在 8 级电机中的动作。
// +1 = 正转 (向下级放行)
// -1 = 反转 (向本地槽位剔除)
//  0 = 不动作 (MOTOR_NO_MOVE)
extern const int8_t ROUTING_TABLE[MAX_TARGETS + 1][NUM_MOTORS];

// --- 步进电机几何参数 ---
#define MOTOR_FULL_STEPS      200   // 每转整步数（1.8°/步电机）
#define GEAR_RATIO            4     // 减速比 1:4（电机转 4 圈，分拣轮转 1 圈）
// 分拣轮旋转 90° 所需步数：(整步数 × 细分 × 减速比) / 4 = 3200
#define STEPS_PER_90DEG  (MOTOR_FULL_STEPS * MICROSTEP_RESOLUTION * GEAR_RATIO / 4)

// --- 运动参数 ---
#define STEPPER_MAX_SPEED     6400.0f   // 最大速度（步/秒）
#define STEPPER_ACCELERATION  6400.0f   // 加速度（步/秒²）
#define HOMING_CONSTANT_SPEED (-800.0f) // 归零恒速（步/秒，负值 = 朝向限位开关方向）
#define DIAG_MOTOR_SPEED      1600.0f   // 诊断模式电机最大速度（步/秒）
#define DIAG_MOTOR_ACCEL      3200.0f   // 诊断模式电机加速度（步/秒²）

// --- 各轴电机方向 (1 = 正向，-1 = 反向) ---
// 为了兼容8电机架构，这里暂不使用宏，而是在 SorterController 中统一定义为常量数组。

// --- 逻辑位置（方向系数乘入之前的步数值）---
#define POS_NEUTRAL       0L
#define ROTATE_LEFT_POS   (-(long)STEPS_PER_90DEG)  // 向左旋转 90°
#define ROTATE_RIGHT_POS  ( (long)STEPS_PER_90DEG)  // 向右旋转 90°

// --- 时序参数 ---
#define SLIDE_WAIT_MS   1  // 等待物品滑行完成的时间（毫秒）
#define DEBOUNCE_MS       50UL  // 按键防抖采样间隔（毫秒）

// --- 哨兵值：表示该电机本节拍无需移动 ---
// 分拣轮具有 90° 旋转对称性，任意停止位置均等价于中立位。
#define MOTOR_NO_MOVE  LONG_MIN

#endif // CONFIG_H
