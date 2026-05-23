#include "SorterController.h"
#include "pins.h"
#include "Logger.h"
#include "apps/AppProduction.h"
#include "apps/AppMotorDiag.h"
#include "apps/AppHallDiag.h"
#include <Arduino.h>

// 定义全局的路由配置表
const int8_t ROUTING_TABLE[MAX_TARGETS + 1][NUM_MOTORS] = {
  // M0, M1, M2, M3, M4, M5, M6, M7
  { 0,  0,  0,  0,  0,  0,  0,  0}, // Target 0: 空槽 / 异常丢弃
  {-1,  0,  0,  0,  0,  0,  0,  0}, // Target 1: 剔除于 M0
  { 1, -1,  0,  0,  0,  0,  0,  0}, // Target 2: 放行于 M0, 剔除于 M1
  { 1,  1, -1,  0,  0,  0,  0,  0}, // Target 3: 放行至 M2, 剔除于 M2
  { 1,  1,  1, -1,  0,  0,  0,  0}, // Target 4: 放行至 M3, 剔除于 M3
  { 1,  1,  1,  1, -1,  0,  0,  0}, // Target 5: 放行至 M4, 剔除于 M4
  { 1,  1,  1,  1,  1, -1,  0,  0}, // Target 6: 放行至 M5, 剔除于 M5
  { 1,  1,  1,  1,  1,  1, -1,  0}, // Target 7: 放行至 M6, 剔除于 M6
  { 1,  1,  1,  1,  1,  1,  1, -1}  // Target 8: 放行至 M7, 剔除于 M7
};

// ============================================================================
// 构造与析构函数
// ============================================================================

SorterController::SorterController(AccelStepper& sharedStepper)
  : _motorHardware(sharedStepper, (const uint8_t[]){EN_PIN_0, EN_PIN_1, EN_PIN_2, EN_PIN_3, EN_PIN_4, EN_PIN_5, EN_PIN_6, EN_PIN_7}, NUM_MOTORS),
    _spiBus(LATCH_PIN, CLOCK_PIN, DIR_DATA_OUT, HOME_DATA_IN),
    _entranceSensor(ENTRANCE_SENSOR_PIN, DEBOUNCE_MS),
    _planner(),
    _homing(_motorHardware, _spiBus),
    _state(IDLE), _timerMark(0), _errorCode(ERR_NONE)
{
  int md[NUM_MOTORS] = { -1, -1, 1, 1, 1, 1, 1, 1 };
  for (int i = 0; i < NUM_MOTORS; i++) {
    _pipeline[i] = 0;
    _motorDirs[i] = md[i];
  }

  // 实例化各 App，默认激活正常生产模式
  _appProduction = new AppProduction(*this);
  _appMotorDiag = new AppMotorDiag(*this);
  _appHallDiag = new AppHallDiag(*this);
  _activeApp = _appProduction;
}

SorterController::~SorterController() {
  delete _appProduction;
  delete _appMotorDiag;
  delete _appHallDiag;
}

// ============================================================================
// begin() —— 初始化
// ============================================================================

void SorterController::begin() {
  _spiBus.begin();
  _motorHardware.begin(STEPPER_MAX_SPEED, STEPPER_ACCELERATION);
  _entranceSensor.begin();

  // 启动当前默认的 App
  if (_activeApp) {
    _activeApp->begin();
  }

  LOG_I("SorterController 初始化完成");
}

// ============================================================================
// 公开接口
// ============================================================================

void SorterController::queueTarget(int targetID) {
  if (_state != DIAG_MOTOR && _state != DIAG_HALL) {
    _queue.push(targetID);
  } else {
    LOG_W("处于诊断模式，忽略入队目标 ID: %d", targetID);
  }
}

void SorterController::triggerHoming() {
  switchApp(HOMING); // 切换回生产模式并开始归零流程
  LOG_I("进入归零模式");
  _queue.flush();
  for (int i = 0; i < NUM_MOTORS; i++) _pipeline[i] = 0;
  
  _homing.start();
}

void SorterController::switchApp(State newState) {
  if (_state == newState) return;

  LOG_I("SorterController: 切换模式从 %d 至 %d", _state, newState);

  // 结束当前 APP 的生命周期
  if (_activeApp) {
    _activeApp->end();
  }

  _state = newState;
  
  // 根据新状态，指派对应的逻辑执行体 APP
  if (_state == DIAG_MOTOR) {
    _activeApp = _appMotorDiag;
  } else if (_state == DIAG_HALL) {
    _activeApp = _appHallDiag;
  } else {
    _activeApp = _appProduction;
  }

  // 启动新 APP 的初始化
  if (_activeApp) {
    _activeApp->begin();
  }
}

void SorterController::handleShortPress() {
  if (_state == DIAG_MOTOR) {
    _appMotorDiag->nextMotor();
  }
}

void SorterController::clearError() {
  LOG_I("SorterController: 清除错误状态，尝试重新同步归零...");
  _errorCode = ERR_NONE;
  triggerHoming();
}

// ============================================================================
// update() —— 状态机驱动
// ============================================================================

void SorterController::update() {
  // 始终驱动步进电机进行运动脉冲分配，供各个 App 共享底层脉冲生成器
  _motorHardware.run();

  // 委托给当前活跃的 APP 执行其专属逻辑
  if (_activeApp) {
    _activeApp->update();
  }
}

// ============================================================================
// 私有辅助函数
// ============================================================================

void SorterController::_advancePipeline() {
  for (int i = NUM_MOTORS - 1; i > 0; i--) {
    _pipeline[i] = _pipeline[i - 1];
  }
  _pipeline[0] = _queue.pop();
}

void SorterController::_printPipelineState() const {
  Serial.printf("[D] 流水线: ");
  for (int i = 0; i < NUM_MOTORS; i++) {
    Serial.printf("[M%d:%d] ", i, _pipeline[i]);
  }
  Serial.println();
}
