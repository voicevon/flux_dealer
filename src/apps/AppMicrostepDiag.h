#ifndef APP_MICROSTEP_DIAG_H
#define APP_MICROSTEP_DIAG_H

#include "apps/AppBase.h"
#include "config.h"

// --- 细分诊断速度与加速度基准 (1 细分下的速度，步/秒) ---
#define MICROSTEP_BASE_SPEED  200.0f
#define MICROSTEP_BASE_ACCEL  400.0f

class FluxDealer;

class AppMicrostepDiag : public AppBase {
public:
  AppMicrostepDiag(FluxDealer& controller);
  void begin() override;
  void update() override;
  void end() override;

  // 短按时触发：切换到下一个细分
  void nextMicrostep();

private:
  void _applyMicrostep(int ms);

  FluxDealer& _controller;
  int _currentMicrostep;
  int _pendingMicrostep;
  bool _microstepChangePending;

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

#endif // APP_MICROSTEP_DIAG_H
