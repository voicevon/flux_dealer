#ifndef CONFIG_H
#define CONFIG_H

#include <limits.h>

// ============================================================================
// 硬件配置：MKS BASE V1.6 / RAMPS 1.4（ATmega2560）
// ============================================================================

// --- 细分配置（须与驱动板上 MS1/MS2/MS3 跳线保持一致）---
// A4988 / DRV8825 的细分由硬件跳线决定，软件无法更改。
// 三个跳线全部安装 = 1/16 细分。
#define MICROSTEP_RESOLUTION  16    // 可选：1 | 2 | 4 | 8 | 16

// --- 步进电机几何参数 ---
#define MOTOR_FULL_STEPS      200   // 每转整步数（1.8°/步电机）
#define GEAR_RATIO            4     // 减速比 1:4（电机转 4 圈，分拣轮转 1 圈）
// 分拣轮旋转 90° 所需步数：(整步数 × 细分 × 减速比) / 4 = 3200
#define STEPS_PER_90DEG  (MOTOR_FULL_STEPS * MICROSTEP_RESOLUTION * GEAR_RATIO / 4)

// --- 运动参数 ---
#define STEPPER_MAX_SPEED     6400.0f   // 最大速度（步/秒）
#define STEPPER_ACCELERATION  6400.0f   // 加速度（步/秒²）
#define HOMING_CONSTANT_SPEED (-800.0f) // 归零恒速（步/秒，负值 = 朝向限位开关方向）

// --- 各轴电机方向 ---
// +1 = 正向，-1 = 反向（电机安装方向相反时取反）
#define MOTOR_X_DIR  (-1)   // 第一级分拣电机
#define MOTOR_Y_DIR  (-1)   // 第二级分拣电机
#define MOTOR_Z_DIR  (+1)   // 第三级分拣电机

// --- 逻辑位置（方向系数乘入之前的步数值）---
#define POS_NEUTRAL       0L
#define ROTATE_LEFT_POS   (-(long)STEPS_PER_90DEG)  // 向左旋转 90°
#define ROTATE_RIGHT_POS  ( (long)STEPS_PER_90DEG)  // 向右旋转 90°

// --- 节拍时序 ---
#define SLIDE_WAIT_MS  2000UL   // 等待物品滑行完成的时间（毫秒）

// --- 哨兵值：表示该电机本节拍无需移动 ---
// 分拣轮具有 90° 旋转对称性，任意停止位置均等价于中立位。
#define MOTOR_NO_MOVE  LONG_MIN

#endif // CONFIG_H
