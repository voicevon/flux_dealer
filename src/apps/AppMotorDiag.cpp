#include "apps/AppMotorDiag.h"
#include "SorterController.h"
#include "Logger.h"
#include <Arduino.h>

AppMotorDiag::AppMotorDiag(SorterController& controller) 
  : _controller(controller), _currentMotor(0), _testState(TEST_IDLE), _pauseTimer(0) {}

void AppMotorDiag::begin() {
  LOG_I("--- [诊断模式] 电机诊断启动 ---");
  LOG_I("短按 GPIO 0 按键可在 M0-M7 电机间循环切换");
  
  _currentMotor = 0;
  _testState = TEST_IDLE;
  _pauseTimer = 0;
  
  // 初始屏蔽所有电机，复位位置
  _controller._motorHardware.setEnableMask(0x00);
  _controller._motorHardware.stop();
  _controller._motorHardware.setCurrentPosition(0);
}

void AppMotorDiag::end() {
  LOG_I("--- [诊断模式] 电机诊断结束 ---");
  _controller._motorHardware.stop();
  _controller._motorHardware.setEnableMask(0x00);
}

void AppMotorDiag::nextMotor() {
  _currentMotor = (_currentMotor + 1) % NUM_MOTORS;
  LOG_I("[诊断模式] 切换至测试电机 M%d", _currentMotor);
  
  // 停止当前动作并复位状态
  _controller._motorHardware.stop();
  _controller._motorHardware.setCurrentPosition(0);
  _testState = TEST_IDLE;
}

void AppMotorDiag::update() {
  switch (_testState) {
    case TEST_IDLE:
      _testState = TEST_MOVE_RIGHT;
      break;

    case TEST_MOVE_RIGHT:
      LOG_D("[诊断模式] 电机 M%d 向右旋转 90 度", _currentMotor);
      // 设置方向：当前测试电机位为 1 (正转)
      _controller._spiBus.transfer(1 << _currentMotor);
      _controller._motorHardware.setEnableMask(1 << _currentMotor);
      _controller._motorHardware.startMove(STEPS_PER_90DEG);
      _testState = TEST_WAIT_RIGHT;
      break;

    case TEST_WAIT_RIGHT:
      if (!_controller._motorHardware.isMoving()) {
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
      LOG_D("[诊断模式] 电机 M%d 向左旋转 90 度", _currentMotor);
      // 设置方向：所有位为 0 (反转)
      _controller._spiBus.transfer(0);
      _controller._motorHardware.setEnableMask(1 << _currentMotor);
      _controller._motorHardware.startMove(STEPS_PER_90DEG);
      _testState = TEST_WAIT_LEFT;
      break;

    case TEST_WAIT_LEFT:
      if (!_controller._motorHardware.isMoving()) {
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
