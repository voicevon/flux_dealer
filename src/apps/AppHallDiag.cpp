#include "apps/AppHallDiag.h"
#include "SorterController.h"
#include "pins.h"
#include "Logger.h"
#include <Arduino.h>

AppHallDiag::AppHallDiag(SorterController& controller) 
  : _controller(controller), _lastPrintTime(0), _lastHomeState(0xFF) {}

void AppHallDiag::begin() {
  LOG_I("--- [诊断模式] 霍尔限位诊断启动 ---");
  LOG_I("用磁铁靠近各轴霍尔开关进行测试。当任意传感器被触发时，板载 LED 将会亮起。");
  
  _lastPrintTime = 0;
  _lastHomeState = 0xFF;
  
  // 诊断霍尔时屏蔽所有电机脉冲，保障安全
  _controller._motorHardware.setEnableMask(0x00);
  _controller._motorHardware.stop();
}

void AppHallDiag::end() {
  LOG_I("--- [诊断模式] 霍尔限位诊断结束 ---");
  // 恢复 LED 电平为低电平（默认状态）
  digitalWrite(LED_PIN, LOW);
}

void AppHallDiag::update() {
  // 确保处于屏蔽状态
  _controller._motorHardware.setEnableMask(0x00);
  
  // 从 SPI 总线（74HC165）读取当前霍尔状态
  uint8_t home_state = _controller._spiBus.transfer(0x00);
  
  // 如果状态发生改变，或者超过 500ms，则打印一次状态
  if (home_state != _lastHomeState || millis() - _lastPrintTime >= 500) {
    _lastPrintTime = millis();
    _lastHomeState = home_state;
    
    Serial.print("[诊断模式] 霍尔状态: ");
    for (int i = 0; i < NUM_MOTORS; i++) {
      // 霍尔触发时通常被拉低为 0
      bool triggered = ((home_state & (1 << i)) == 0);
      Serial.printf("M%d:%s ", i, triggered ? "TRG" : "---");
    }
    Serial.println();
  }
  
  // 如果有任何一个霍尔传感器被触发 (home_state 不为全 1)，则点亮开发板板载 LED 2 进行实时指示
  bool anyTriggered = (home_state != 0xFF);
  digitalWrite(LED_PIN, anyTriggered ? HIGH : LOW);
}
