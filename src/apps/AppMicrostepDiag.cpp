#include "apps/AppMicrostepDiag.h"
#include "FluxDealer.h"
#include "Logger.h"
#include "pins.h"
#include <Arduino.h>

AppMicrostepDiag::AppMicrostepDiag(FluxDealer& controller)
  : _controller(controller),
    _currentMicrostep(1),
    _pendingMicrostep(1),
    _microstepChangePending(false),
    _testState(TEST_IDLE),
    _pauseTimer(0) {}

void AppMicrostepDiag::begin() {
  LOG_I("--- [诊断模式] 细分诊断启动 ---");
  LOG_I("短按 GPIO 0 按键可循环切换细分 (1 -> 2 -> 4 -> 8 -> 16)");

  _currentMicrostep = 1;
  _pendingMicrostep = 1;
  _microstepChangePending = false;
  _testState = TEST_IDLE;
  _pauseTimer = 0;

  // 统一应用初始细分 1
  _applyMicrostep(_currentMicrostep);

  // 启用所有电机并复位位置
  _controller._motorHardware.setEnableMask(0xFF);
  _controller._motorHardware.stop();
  _controller._motorHardware.setCurrentPosition(0);
}

void AppMicrostepDiag::end() {
  LOG_I("--- [诊断模式] 细分诊断结束 ---");

  // 恢复正常的生产速度、加速度和细分配置
  _controller._motorHardware.setMaxSpeed(STEPPER_MAX_SPEED);
  _controller._motorHardware.setAcceleration(STEPPER_ACCELERATION);

  // 恢复 config.h 中定义的默认细分
  #if MICROSTEP_RESOLUTION == 1
    digitalWrite(MS1_PIN, LOW);
    digitalWrite(MS2_PIN, LOW);
    digitalWrite(MS3_PIN, LOW);
  #elif MICROSTEP_RESOLUTION == 2
    digitalWrite(MS1_PIN, HIGH);
    digitalWrite(MS2_PIN, LOW);
    digitalWrite(MS3_PIN, LOW);
  #elif MICROSTEP_RESOLUTION == 4
    digitalWrite(MS1_PIN, LOW);
    digitalWrite(MS2_PIN, HIGH);
    digitalWrite(MS3_PIN, LOW);
  #elif MICROSTEP_RESOLUTION == 8
    digitalWrite(MS1_PIN, HIGH);
    digitalWrite(MS2_PIN, HIGH);
    digitalWrite(MS3_PIN, LOW);
  #elif MICROSTEP_RESOLUTION == 16
    digitalWrite(MS1_PIN, HIGH);
    digitalWrite(MS2_PIN, HIGH);
    digitalWrite(MS3_PIN, HIGH);
  #endif

  _controller._motorHardware.stop();
  _controller._motorHardware.setEnableMask(0x00);
}

void AppMicrostepDiag::nextMicrostep() {
  int nextMS = 1;
  if (_pendingMicrostep == 1) nextMS = 2;
  else if (_pendingMicrostep == 2) nextMS = 4;
  else if (_pendingMicrostep == 4) nextMS = 8;
  else if (_pendingMicrostep == 8) nextMS = 16;
  else if (_pendingMicrostep == 16) nextMS = 1;

  _pendingMicrostep = nextMS;

  // 如果电机当前正在运转，我们设置挂起标志，等运动结束后再切换
  if (_controller._motorHardware.isMoving()) {
    _microstepChangePending = true;
    LOG_I("[诊断模式] 细分切换至 %d 挂起中，等待当前 90° 旋转结束...", _pendingMicrostep);
  } else {
    // 否则，如果当前是空闲或处于暂停状态，可以直接切换
    _currentMicrostep = _pendingMicrostep;
    _microstepChangePending = false;
    _applyMicrostep(_currentMicrostep);
  }
}

void AppMicrostepDiag::update() {
  long stepsFor90Deg = (long)(MOTOR_FULL_STEPS * _currentMicrostep * GEAR_RATIO / 4.0f);

  switch (_testState) {
    case TEST_IDLE:
      _testState = TEST_MOVE_RIGHT;
      break;

    case TEST_MOVE_RIGHT:
      LOG_D("[诊断模式] 细分 %d: 电机向右旋转 90 度 (%ld 步)", _currentMicrostep, stepsFor90Deg);
      // 设置方向：所有位为 1 (正转)
      _controller._spiBus.transfer(0xFF);
      _controller._motorHardware.setEnableMask(0xFF);
      _controller._motorHardware.startMove(stepsFor90Deg);
      _testState = TEST_WAIT_RIGHT;
      break;

    case TEST_WAIT_RIGHT:
      if (!_controller._motorHardware.isMoving()) {
        if (_microstepChangePending) {
          _currentMicrostep = _pendingMicrostep;
          _microstepChangePending = false;
          _applyMicrostep(_currentMicrostep);
        }
        _controller._motorHardware.setCurrentPosition(0);
        _pauseTimer = millis();
        _testState = TEST_PAUSE_BEFORE_LEFT;
      }
      break;

    case TEST_PAUSE_BEFORE_LEFT:
      if (millis() - _pauseTimer >= 500) {
        _testState = TEST_MOVE_LEFT;
      }
      break;

    case TEST_MOVE_LEFT:
      LOG_D("[诊断模式] 细分 %d: 电机向左旋转 90 度 (%ld 步)", _currentMicrostep, stepsFor90Deg);
      // 设置方向：所有位为 0 (反转)
      _controller._spiBus.transfer(0x00);
      _controller._motorHardware.setEnableMask(0xFF);
      _controller._motorHardware.startMove(-stepsFor90Deg);
      _testState = TEST_WAIT_LEFT;
      break;

    case TEST_WAIT_LEFT:
      if (!_controller._motorHardware.isMoving()) {
        if (_microstepChangePending) {
          _currentMicrostep = _pendingMicrostep;
          _microstepChangePending = false;
          _applyMicrostep(_currentMicrostep);
        }
        _controller._motorHardware.setCurrentPosition(0);
        _pauseTimer = millis();
        _testState = TEST_PAUSE_BEFORE_RIGHT;
      }
      break;

    case TEST_PAUSE_BEFORE_RIGHT:
      if (millis() - _pauseTimer >= 500) {
        _testState = TEST_MOVE_RIGHT;
      }
      break;

    default:
      break;
  }
}

void AppMicrostepDiag::_applyMicrostep(int ms) {
  LOG_I("[诊断模式] 应用细分设置: %d 细分", ms);
  if (ms == 1) {
    digitalWrite(MS1_PIN, LOW);
    digitalWrite(MS2_PIN, LOW);
    digitalWrite(MS3_PIN, LOW);
  } else if (ms == 2) {
    digitalWrite(MS1_PIN, HIGH);
    digitalWrite(MS2_PIN, LOW);
    digitalWrite(MS3_PIN, LOW);
  } else if (ms == 4) {
    digitalWrite(MS1_PIN, LOW);
    digitalWrite(MS2_PIN, HIGH);
    digitalWrite(MS3_PIN, LOW);
  } else if (ms == 8) {
    digitalWrite(MS1_PIN, HIGH);
    digitalWrite(MS2_PIN, HIGH);
    digitalWrite(MS3_PIN, LOW);
  } else if (ms == 16) {
    digitalWrite(MS1_PIN, HIGH);
    digitalWrite(MS2_PIN, HIGH);
    digitalWrite(MS3_PIN, HIGH);
  }

  // 动态调整电机运行速度与加速度
  float speed = MICROSTEP_BASE_SPEED * ms;
  float accel = MICROSTEP_BASE_ACCEL * ms;
  _controller._motorHardware.setMaxSpeed(speed);
  _controller._motorHardware.setAcceleration(accel);
}
