#ifndef APP_MOTOR_DIAG_H
#define APP_MOTOR_DIAG_H

#include "apps/AppBase.h"

class SorterController;

class AppMotorDiag : public AppBase {
public:
  AppMotorDiag(SorterController& controller);
  void begin() override;
  void update() override;
  void end() override;

  // 短按时触发：切换到下一个电机测试
  void nextMotor();

private:
  SorterController& _controller;
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
