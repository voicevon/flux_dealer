#include "HomingController.h"
#include "Logger.h"

HomingController::HomingController(MotorHardware& motorHardware, ShiftRegisterBus& spiBus)
  : _motorHardware(motorHardware), _spiBus(spiBus), 
    _homingActive(false), _homingStartTime(0), _errorState(false) 
{
  for (int i = 0; i < NUM_MOTORS; i++) {
    _homingDone[i] = false;
  }
}

void HomingController::start() {
  LOG_I("HomingController: 开始同步归零流程...");
  _homingActive = true;
  _errorState = false;
  
  for (int i = 0; i < NUM_MOTORS; i++) {
    _homingDone[i] = false;
  }

  // 1. 设置归零速度
  _motorHardware.setSpeed(HOMING_CONSTANT_SPEED);

  // 2. 强制发送方向全为 0 (朝向归零限位开关方向)
  _spiBus.transfer(0x00);

  // 3. 放行所有轴进行运动
  _motorHardware.setEnableMask(0xFF); // 0xFF 即 8 位全是 1 (全使能)

  _homingStartTime = millis();
}

bool HomingController::update() {
  if (!_homingActive) {
    return false;
  }

  // 超时检测 (10 秒超时)
  if (millis() - _homingStartTime > 10000) {
    LOG_E("HomingController: 归零超时！可能限位开关损坏或轴卡死！");
    _motorHardware.setEnableMask(0x00); // 屏蔽所有电机脉冲，防止硬件损坏
    _motorHardware.stop();
    _errorState = true;
    _homingActive = false;
    return false;
  }

  // 轮询移位寄存器，读取各轴限位状态
  uint8_t home_state = _spiBus.transfer(0x00);
  bool all_done = true;

  for (int i = 0; i < NUM_MOTORS; i++) {
    if (!_homingDone[i]) {
      // 限位触发时通常被拉低为 0
      bool triggered = ((home_state & (1 << i)) == 0);
      if (triggered) {
        _motorHardware.setMotorEnabled(i, false); // 触发后立即屏蔽脉冲 (false = 屏蔽)
        _homingDone[i] = true;
        LOG_I("HomingController: 电机 %d 归零完成", i);
      } else {
        all_done = false;
      }
    }
  }

  if (all_done) {
    _motorHardware.stop();
    _motorHardware.setCurrentPosition(0);
    _homingActive = false;
    LOG_I("HomingController: --- 所有电机成功归零并重置原点 ---");
    return false;
  }

  // 驱动共享脉冲进行恒速运转
  _motorHardware.runSpeed();
  return true;
}

bool HomingController::isHomingDone() const {
  if (_homingActive || _errorState) return false;
  for (int i = 0; i < NUM_MOTORS; i++) {
    if (!_homingDone[i]) return false;
  }
  return true;
}

bool HomingController::hasError() const {
  return _errorState;
}
