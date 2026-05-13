#include "SorterController.h"
#include "pins.h"
#include <Arduino.h>

// ============================================================================
// 构造函数
// ============================================================================

SorterController::SorterController(AccelStepper& motorX,
                                   AccelStepper& motorY,
                                   AccelStepper& motorZ)
  : _x(motorX), _y(motorY), _z(motorZ),
    _state(IDLE), _timerMark(0),
    _pipeline{0, 0, 0},
    _homingInit(false), _xDone(false), _yDone(false), _zDone(false)
{}

// ============================================================================
// begin() —— 一次性硬件初始化
// ============================================================================

void SorterController::begin() {
  // 使能步进驱动器（低电平有效）
  pinMode(X_ENABLE_PIN, OUTPUT); digitalWrite(X_ENABLE_PIN, LOW);
  pinMode(Y_ENABLE_PIN, OUTPUT); digitalWrite(Y_ENABLE_PIN, LOW);
  pinMode(Z_ENABLE_PIN, OUTPUT); digitalWrite(Z_ENABLE_PIN, LOW);

  // 配置 AccelStepper 速度与加速度
  _x.setMaxSpeed(STEPPER_MAX_SPEED); _x.setAcceleration(STEPPER_ACCELERATION);
  _y.setMaxSpeed(STEPPER_MAX_SPEED); _y.setAcceleration(STEPPER_ACCELERATION);
  _z.setMaxSpeed(STEPPER_MAX_SPEED); _z.setAcceleration(STEPPER_ACCELERATION);

  // 限位开关输入（内部上拉）
  pinMode(X_HOME_PIN, INPUT_PULLUP);
  pinMode(Y_HOME_PIN, INPUT_PULLUP);
  pinMode(Z_HOME_PIN, INPUT_PULLUP);
}

// ============================================================================
// 公开接口
// ============================================================================

void SorterController::queueTarget(int targetID) {
  _queue.push(targetID);
}

void SorterController::triggerHoming() {
  Serial.println("检测到多键同时按下 -> 进入归零模式。");
  _queue.flush();                            // 清空待处理队列
  _pipeline[0] = _pipeline[1] = _pipeline[2] = 0; // 清空流水线
  _state = HOMING;
}

// ============================================================================
// update() —— 每次 loop() 调用
// ============================================================================

void SorterController::update() {
  // 始终驱动步进电机（保持脉冲连续性）
  _runAll();

  switch (_state) {

    case IDLE:
      // 队列非空或流水线仍有物品时，启动新节拍
      if (!_queue.empty() ||
          _pipeline[0] != 0 || _pipeline[1] != 0 || _pipeline[2] != 0) {
        _state = NEW_BEAT_PREP;
      }
      break;

    case NEW_BEAT_PREP:
      Serial.println("\n--- 新节拍开始 ---");
      _advancePipeline();    // 流水线前移，弹入新目标
      _printPipelineState(); // 打印当前流水线状态

      // 向各轴下发移动指令；MOTOR_NO_MOVE 的轴保持原位。
      _applyMove(_x, _getTargetPos(0, _pipeline[0]));
      _applyMove(_y, _getTargetPos(1, _pipeline[1]));
      _applyMove(_z, _getTargetPos(2, _pipeline[2]));

      _state = MOVING_TO_ROUTE;
      break;

    case MOVING_TO_ROUTE:
      // 等待所有轴到位
      if (!_anyRunning()) {
        Serial.println("已到达分拣位置，等待物品滑行...");
        _timerMark = millis();
        _state = SLIDING_WAIT;
      }
      break;

    case SLIDING_WAIT:
      // 物品在重力滑道上自然滑行，等待固定时间后继续
      if (millis() - _timerMark >= SLIDE_WAIT_MS) {
        // 分拣轮 90° 对称——电机留在当前位置，等下次节拍再移动。
        Serial.println("滑行等待结束，本节拍完成。");
        _state = COMPLETED_BEAT;
      }
      break;

    case COMPLETED_BEAT:
      // 节拍冷却完毕，回到空闲
      _state = IDLE;
      break;

    case HOMING:
      _doHomingStep();
      break;
  }
}

// ============================================================================
// 私有辅助函数
// ============================================================================

long SorterController::_getTargetPos(int stage, int targetID) const {
  if (targetID == 0) return MOTOR_NO_MOVE; // 槽位为空，不移动

  switch (stage) {
    case 0: // 电机 X —— 第一级分拣
      return (targetID == 1)
        ? MOTOR_X_DIR * ROTATE_LEFT_POS   // 目标 1：向左
        : MOTOR_X_DIR * ROTATE_RIGHT_POS; // 目标 2/3/4：向右

    case 1: // 电机 Y —— 第二级分拣
      if (targetID == 1) return MOTOR_NO_MOVE; // 目标 1 已在第 0 级分出
      return (targetID == 2)
        ? MOTOR_Y_DIR * ROTATE_RIGHT_POS  // 目标 2：向右
        : MOTOR_Y_DIR * ROTATE_LEFT_POS;  // 目标 3/4：向左

    case 2: // 电机 Z —— 第三级分拣
      if (targetID == 1 || targetID == 2) return MOTOR_NO_MOVE; // 已在上级分出
      return (targetID == 3)
        ? MOTOR_Z_DIR * ROTATE_LEFT_POS   // 目标 3：向左
        : MOTOR_Z_DIR * ROTATE_RIGHT_POS; // 目标 4：向右

    default:
      return MOTOR_NO_MOVE;
  }
}

void SorterController::_applyMove(AccelStepper& s, long target) {
  // 仅当目标有效时才下发相对移动（跳过 MOTOR_NO_MOVE 避免多余回转）
  if (target != MOTOR_NO_MOVE) {
    s.move(target - s.currentPosition());
  }
}

void SorterController::_runAll() {
  _x.run();
  _y.run();
  _z.run();
}

bool SorterController::_anyRunning() const {
  return _x.isRunning() || _y.isRunning() || _z.isRunning();
}

void SorterController::_advancePipeline() {
  // 将各级物品向下游推进一格，从队列弹入新物品
  _pipeline[2] = _pipeline[1];
  _pipeline[1] = _pipeline[0];
  _pipeline[0] = _queue.pop();
}

void SorterController::_printPipelineState() const {
  Serial.print("流水线状态：[M1 目标 "); Serial.print(_pipeline[0]);
  Serial.print("] -> [M2 目标 ");         Serial.print(_pipeline[1]);
  Serial.print("] -> [M3 目标 ");         Serial.print(_pipeline[2]);
  Serial.println("]");
}

void SorterController::_doHomingStep() {
  if (!_homingInit) {
    Serial.println("开始同步归零...");
    _xDone = _yDone = _zDone = false;
    // 以恒速直接驱动（绕过加速曲线），方向为负（朝向限位开关）
    _x.setSpeed(HOMING_CONSTANT_SPEED);
    _y.setSpeed(HOMING_CONSTANT_SPEED);
    _z.setSpeed(HOMING_CONSTANT_SPEED);
    _homingInit = true;
  }

  // X 轴归零
  if (!_xDone) {
    if (digitalRead(X_HOME_PIN) == LOW) {
      _x.stop(); _x.setCurrentPosition(0);
      _xDone = true;
      Serial.println("X 轴归零完成。");
    } else { _x.runSpeed(); }
  }

  // Y 轴归零
  if (!_yDone) {
    if (digitalRead(Y_HOME_PIN) == LOW) {
      _y.stop(); _y.setCurrentPosition(0);
      _yDone = true;
      Serial.println("Y 轴归零完成。");
    } else { _y.runSpeed(); }
  }

  // Z 轴归零
  if (!_zDone) {
    if (digitalRead(Z_HOME_PIN) == LOW) {
      _z.stop(); _z.setCurrentPosition(0);
      _zDone = true;
      Serial.println("Z 轴归零完成。");
    } else { _z.runSpeed(); }
  }

  // 全部完成后重置并回到空闲
  if (_xDone && _yDone && _zDone) {
    Serial.println("--- 所有电机归零并重置原点 ---");
    _homingInit = false;
    _state = IDLE;
  }
}
