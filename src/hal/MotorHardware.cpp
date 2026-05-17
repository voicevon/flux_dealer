#include "hal/MotorHardware.h"

MotorHardware::MotorHardware(AccelStepper& sharedStepper, const uint8_t* enablePins, uint8_t numMotors)
  : _sharedStepper(sharedStepper), _numMotors(numMotors) 
{
  _enablePins = new uint8_t[_numMotors];
  for (int i = 0; i < _numMotors; i++) {
    _enablePins[i] = enablePins[i];
  }
}

MotorHardware::~MotorHardware() {
  delete[] _enablePins;
}

void MotorHardware::begin(float maxSpeed, float acceleration) {
  // 初始化所有 EN 引脚为屏蔽状态 (HIGH)
  for (int i = 0; i < _numMotors; i++) {
    pinMode(_enablePins[i], OUTPUT);
    digitalWrite(_enablePins[i], HIGH);
  }

  _sharedStepper.setMaxSpeed(maxSpeed); 
  _sharedStepper.setAcceleration(acceleration);
}

void MotorHardware::setEnableMask(uint8_t enableMask) {
  for (int i = 0; i < _numMotors; i++) {
    // 1 表示放行 (LOW), 0 表示屏蔽 (HIGH)
    bool enabled = (enableMask & (1 << i)) != 0;
    digitalWrite(_enablePins[i], enabled ? LOW : HIGH);
  }
}

void MotorHardware::setMotorEnabled(uint8_t motorIndex, bool enabled) {
  if (motorIndex < _numMotors) {
    digitalWrite(_enablePins[motorIndex], enabled ? LOW : HIGH);
  }
}

void MotorHardware::startMove(long steps) {
  _sharedStepper.move(steps);
}

void MotorHardware::stop() {
  _sharedStepper.stop();
}

bool MotorHardware::isMoving() const {
  return _sharedStepper.isRunning();
}

void MotorHardware::setCurrentPosition(long pos) {
  _sharedStepper.setCurrentPosition(pos);
}

void MotorHardware::setSpeed(float speed) {
  _sharedStepper.setSpeed(speed);
}

void MotorHardware::run() {
  _sharedStepper.run();
}

void MotorHardware::runSpeed() {
  _sharedStepper.runSpeed();
}
