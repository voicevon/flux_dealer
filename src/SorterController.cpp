#include "SorterController.h"
#include "pins.h"
#include "Logger.h"
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
// 构造函数
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
}

// ============================================================================
// begin() —— 初始化
// ============================================================================

void SorterController::begin() {
  _spiBus.begin();
  _motorHardware.begin(STEPPER_MAX_SPEED, STEPPER_ACCELERATION);
  _entranceSensor.begin();

  LOG_I("SorterController 初始化完成");
}

// ============================================================================
// 公开接口
// ============================================================================

void SorterController::queueTarget(int targetID) {
  _queue.push(targetID);
}

void SorterController::triggerHoming() {
  LOG_I("进入归零模式");
  _queue.flush();
  for (int i = 0; i < NUM_MOTORS; i++) _pipeline[i] = 0;
  
  _homing.start();
  _state = HOMING;
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
  // 始终驱动步进电机进行运动脉冲分配
  _motorHardware.run();

  switch (_state) {

    case IDLE: {
      _entranceSensor.update();
      if (_entranceSensor.isRisingEdge()) {
        _state = NEW_BEAT_PREP;
      }
      break;
    }

    case NEW_BEAT_PREP:
      LOG_I("--- 新节拍开始 ---");
      _advancePipeline();
      _printPipelineState();

      // 规划并下发组运动
      {
        MotionCommand cmd = _planner.planGroupMove(_pipeline, _motorDirs);
        _motorHardware.setEnableMask(cmd.enable_mask);
        _spiBus.transfer(cmd.dir_state);

        if (cmd.move_any) {
          _motorHardware.startMove(cmd.steps);
        }
      }

      _state = MOVING_TO_ROUTE;
      break;

    case MOVING_TO_ROUTE:
      if (!_motorHardware.isMoving()) {
        LOG_D("已到达分拣位置，等待物品滑行...");
        _motorHardware.setCurrentPosition(0);
        _timerMark = millis();
        _state = SLIDING_WAIT;
      }
      break;

    case SLIDING_WAIT:
      if (millis() - _timerMark >= SLIDE_WAIT_MS) {
        LOG_D("滑行等待结束，本节拍完成");
        _state = COMPLETED_BEAT;
      }
      break;

    case COMPLETED_BEAT:
      _state = IDLE;
      break;

    case HOMING:
      if (!_homing.update()) {
        if (_homing.hasError()) {
          _errorCode = ERR_HOMING_TIMEOUT;
          _state = ERROR_STATE;
        } else if (_homing.isHomingDone()) {
          _state = IDLE;
        }
      }
      break;

    case ERROR_STATE:
      // 等待外部复位（或喂狗）
      break;
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
