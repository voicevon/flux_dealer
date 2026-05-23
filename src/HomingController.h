#ifndef HOMING_CONTROLLER_H
#define HOMING_CONTROLLER_H

#include <Arduino.h>
#include "hal/MotorHardware.h"
#include "hal/ShiftRegisterBus.h"
#include "config.h"

class HomingController {
public:
  HomingController(MotorHardware& motorHardware, ShiftRegisterBus& spiBus);

  // 启动同步归零流程
  void start();

  // 周期性更新归零状态机（在主 loop 中轮询）
  // @return true if 归零流程进行中，false if 归零已结束（成功或发生超时错误）
  bool update();

  // 检查是否所有轴均成功归零
  bool isHomingDone() const;

  // 检查是否发生归零超时/异常
  bool hasError() const;

private:
  MotorHardware& _motorHardware;
  ShiftRegisterBus& _spiBus;

  bool _homingActive;
  bool _homingDone[NUM_MOTORS];
  unsigned long _homingStartTime;
  bool _errorState;
};

#endif // HOMING_CONTROLLER_H
