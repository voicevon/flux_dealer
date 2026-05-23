#ifndef MOTOR_HARDWARE_H
#define MOTOR_HARDWARE_H

#include <Arduino.h>
#include <AccelStepper.h>

class MotorHardware {
public:
  MotorHardware(AccelStepper& sharedStepper, const uint8_t* enablePins, uint8_t numMotors);
  ~MotorHardware();

  void begin(float maxSpeed, float acceleration);
  
  // 设置全体电机使能屏蔽：mask 中位 i 为 1 表示电机 i 启用（放行，引脚 LOW），0 表示屏蔽（引脚 HIGH）
  void setEnableMask(uint8_t enableMask);
  
  // 单一电机设置使能状态 (true = 放行/LOW, false = 屏蔽/HIGH)
  void setMotorEnabled(uint8_t motorIndex, bool enabled);

  // 步进控制代理方法
  void startMove(long steps);
  void stop();
  bool isMoving() const;
  void setCurrentPosition(long pos);
  void setSpeed(float speed);
  void run();
  void runSpeed();

private:
  AccelStepper& _sharedStepper;
  uint8_t* _enablePins;
  uint8_t _numMotors;
};

#endif // MOTOR_HARDWARE_H
