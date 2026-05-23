#include <Arduino.h>
#include <AccelStepper.h>
#include <esp_task_wdt.h>
#include "pins.h"
#include "SorterController.h"
#include "BleManager.h"
#include "Logger.h"

// ============================================================================
// 硬件：ESP32
// 单一共享 STEP 引脚用于产生同步脉冲，独立 DIR 和 EN(脉冲屏蔽)
// ============================================================================
AccelStepper sharedStepper(AccelStepper::DRIVER, SHARED_STEP_PIN, DUMMY_DIR_PIN);

SorterController sorter(sharedStepper);
BleManager bleManager;

// 看门狗超时时间（秒）
#define WDT_TIMEOUT_S  10

// ============================================================================
// 初始化 / 主循环
// ============================================================================
void setup() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(0, INPUT_PULLUP); // BOOT 按键输入，外部带上拉

  Serial.begin(115200);
  while (!Serial) {}   // 等待串口就绪

  LOG_I("--- 分拣机迷你控制器 ---");
  LOG_I("硬件：ESP32");

  // 启用看门狗定时器
  esp_task_wdt_init(WDT_TIMEOUT_S, true);
  esp_task_wdt_add(NULL);
  LOG_I("看门狗已启用 (超时 %d 秒)", WDT_TIMEOUT_S);

  sorter.begin();
  bleManager.begin(&sorter);

  LOG_I("初始化完成，等待 sorter_mini_phone 连接或传感器触发...");
}

void loop() {
  // 喂看门狗
  esp_task_wdt_reset();

  // GPIO 0 按键检测逻辑（防抖，长按切换模式，短按传递给诊断模块）
  static unsigned long buttonPressTime = 0;
  static bool longPressTriggered = false;
  static bool lastButtonState = HIGH;

  bool currentButtonState = digitalRead(0);
  if (currentButtonState != lastButtonState) {
    delay(10); // 10ms 消抖
    currentButtonState = digitalRead(0);
  }

  if (currentButtonState == LOW) {
    if (buttonPressTime == 0) {
      buttonPressTime = millis();
      longPressTriggered = false;
    } else {
      unsigned long duration = millis() - buttonPressTime;
      // 长按 1.5s 切换 APP
      if (duration >= 1500 && !longPressTriggered) {
        longPressTriggered = true;
        SorterController::State currentState = sorter.getState();
        SorterController::State nextState;
        
        if (currentState == SorterController::DIAG_MOTOR) {
          nextState = SorterController::DIAG_HALL;
        } else if (currentState == SorterController::DIAG_HALL) {
          nextState = SorterController::IDLE; // 返回正常模式
        } else {
          nextState = SorterController::DIAG_MOTOR;
        }
        sorter.switchApp(nextState);
      }
    }
  } else {
    if (buttonPressTime != 0) {
      unsigned long duration = millis() - buttonPressTime;
      if (!longPressTriggered && duration >= 50) {
        // 短按触发诊断模式下的换向/换电机等功能
        sorter.handleShortPress();
      }
      buttonPressTime = 0;
      longPressTriggered = false;
    }
  }
  lastButtonState = currentButtonState;

  // 维护之前的状态以决定是否通知蓝牙
  static SorterController::State lastState = SorterController::IDLE;
  static SorterController::ErrorCode lastError = SorterController::ERR_NONE;
  
  // 心跳 / 物理识别指示灯闪烁控制 (仅在非霍尔诊断模式下闪烁，霍尔诊断由 AppHallDiag 独占)
  if (sorter.getState() != SorterController::DIAG_HALL) {
    static unsigned long lastBeat = 0;
    unsigned long blinkInterval = bleManager.isIdentifying() ? 50 : 1000; // 识别期 10Hz 快闪，平时 1Hz 慢闪
    if (millis() - lastBeat >= blinkInterval) {
      lastBeat = millis();
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    }
  }

  sorter.update();  // 驱动状态机与步进电机
  
  // 状态改变时，通知蓝牙
  SorterController::State currentState = sorter.getState();
  if (currentState != lastState) {
    bleManager.updateStatus(currentState);
    lastState = currentState;
  }

  // 错误码改变时，通知蓝牙
  SorterController::ErrorCode currentError = sorter.getErrorCode();
  if (currentError != lastError) {
    bleManager.updateError(currentError);
    lastError = currentError;
  }
}
