#ifndef SENSOR_INPUT_H
#define SENSOR_INPUT_H

#include <Arduino.h>

class SensorInput {
public:
  // @param pin: 光电传感器引脚
  // @param debounceMs: 防抖时间（毫秒）
  SensorInput(uint8_t pin, unsigned long debounceMs);
  
  void begin();
  
  // 需要在每次主循环中调用以更新状态
  void update();
  
  // 传感器是否正被遮挡 (假设低电平触发，返回 true)
  bool isTriggered() const;
  
  // 是否在本次 update 中检测到了有效的触发沿 (刚被遮挡)
  bool isRisingEdge() const;

private:
  uint8_t _pin;
  unsigned long _debounceMs;
  
  bool _currentState;
  bool _lastState;
  bool _risingEdge;
  
  unsigned long _lastChangeTime;
  bool _rawState;
};

#endif // SENSOR_INPUT_H
