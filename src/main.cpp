#include <Arduino.h>
#include <AccelStepper.h>
#include "pins.h"
#include "SorterController.h"
#include "ButtonScanner.h"

// ============================================================================
// 硬件：MKS BASE V1.6（ATmega2560）
// 三路步进驱动器，均使用 DRIVER（脉冲/方向）模式。
// ============================================================================
AccelStepper stepperX(AccelStepper::DRIVER, X_STEP_PIN, X_DIR_PIN);
AccelStepper stepperY(AccelStepper::DRIVER, Y_STEP_PIN, Y_DIR_PIN);
AccelStepper stepperZ(AccelStepper::DRIVER, Z_STEP_PIN, Z_DIR_PIN);

SorterController sorter(stepperX, stepperY, stepperZ);

// ============================================================================
// 按钮事件回调（传入 ButtonScanner 的自由函数）
// ============================================================================
void onTargetPressed(int targetID) { sorter.queueTarget(targetID); }
void onHomingTriggered()           { sorter.triggerHoming();        }

ButtonScanner buttons(onTargetPressed, onHomingTriggered);

// ============================================================================
// 初始化 / 主循环
// ============================================================================
void setup() {
  pinMode(LED_PIN, OUTPUT);

  Serial.begin(115200);
  while (!Serial) {}   // 等待串口就绪

  Serial.println("--- 分拣机迷你控制器 ---");
  Serial.println("硬件：MKS BASE V1.6（ATmega2560）");

  sorter.begin();
  buttons.begin();

  Serial.println("初始化完成，等待指令。");
}

void loop() {
  // 心跳 LED（1 Hz 闪烁）
  static unsigned long lastBeat = 0;
  if (millis() - lastBeat >= 1000) {
    lastBeat = millis();
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }

  buttons.scan();   // 防抖扫描，派发按钮事件
  sorter.update();  // 驱动状态机与步进电机
}
