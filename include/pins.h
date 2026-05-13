#ifndef PINS_H
#define PINS_H

// ============================================================================
// MKS BASE V1.6 / RAMPS 1.4 引脚定义（ATmega2560）
// ============================================================================

// --- X 轴步进电机 ---
#define X_STEP_PIN         54
#define X_DIR_PIN          55
#define X_ENABLE_PIN       38

// --- Y 轴步进电机 ---
#define Y_STEP_PIN         60
#define Y_DIR_PIN          61
#define Y_ENABLE_PIN       56

// --- Z 轴步进电机 ---
#define Z_STEP_PIN         46
#define Z_DIR_PIN          48
#define Z_ENABLE_PIN       62

// --- 挤出机 E0（备用）---
#define E0_STEP_PIN        26
#define E0_DIR_PIN         28
#define E0_ENABLE_PIN      24

// --- 挤出机 E1（备用）---
#define E1_STEP_PIN        36
#define E1_DIR_PIN         34
#define E1_ENABLE_PIN      30

// --- 限位开关 / 归零传感器 ---
// 标准 RAMPS 布局：- (Min) 和 + (Max)
#define X_MIN_PIN           3
#define X_MAX_PIN           2   // 板上标注 "X+"

#define Y_MIN_PIN          14
#define Y_MAX_PIN          15   // 板上标注 "Y+"

#define Z_MIN_PIN          18
#define Z_MAX_PIN          19   // 板上标注 "Z+"

// 归零传感器别名（使用最大位限位开关）
#define X_HOME_PIN         X_MAX_PIN
#define Y_HOME_PIN         Y_MAX_PIN
#define Z_HOME_PIN         Z_MAX_PIN

// --- 目标选择按钮（使用板载伺服接口引脚 D11、D12、A11、A12）---
#define BTN_TARGET_3_PIN   11   // D11
#define BTN_TARGET_4_PIN   12   // D12
#define BTN_TARGET_1_PIN   65   // A11（ATmega2560 数字引脚 65）
#define BTN_TARGET_2_PIN   66   // A12（ATmega2560 数字引脚 66）

// --- 其他 ---
#define LED_PIN            13   // Mega 板载 LED

#endif // PINS_H
