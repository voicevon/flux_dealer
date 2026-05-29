#ifndef APP_MOTOR_DIAG_H
#define APP_MOTOR_DIAG_H

#include "apps/AppBase.h"
#include "config.h"

// --- 电机诊断配置 ---
#define DIAG_TARGET_ROTATIONS 3   // 诊断模式下执行机构（分拣轮）目标旋转圈数
#define DIAG_STEPS  (MOTOR_FULL_STEPS * MICROSTEP_RESOLUTION * GEAR_RATIO * DIAG_TARGET_ROTATIONS)
#define DIAG_MOTOR_SPEED      400.0f   // 诊断模式电机最大速度（步/秒）
#define DIAG_MOTOR_ACCEL      800.0f   // 诊断模式电机加速度（步/秒²）

class FluxDealer;

class AppMotorDiag : public AppBase {
public:
  AppMotorDiag(FluxDealer& controller);
  void begin() override;
  void update() override;
  void end() override;

  // 短按时触发：切换到下一个电机测试
  void nextMotor();

private:
  FluxDealer& _controller;
  int _currentMotor;

  enum TestState {
    TEST_IDLE,
    TEST_MOVE_RIGHT,
    TEST_WAIT_RIGHT,
    TEST_PAUSE_BEFORE_LEFT,
    TEST_MOVE_LEFT,
    TEST_WAIT_LEFT,
    TEST_PAUSE_BEFORE_RIGHT
  };

  TestState _testState;
  unsigned long _pauseTimer;
};

#endif // APP_MOTOR_DIAG_H
