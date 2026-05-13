#include "SorterController.h"
#include "pins.h"
#include <Arduino.h>

// ============================================================================
// Construction
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
// begin() — one-time hardware initialisation
// ============================================================================

void SorterController::begin() {
  // Enable stepper drivers (active LOW)
  pinMode(X_ENABLE_PIN, OUTPUT); digitalWrite(X_ENABLE_PIN, LOW);
  pinMode(Y_ENABLE_PIN, OUTPUT); digitalWrite(Y_ENABLE_PIN, LOW);
  pinMode(Z_ENABLE_PIN, OUTPUT); digitalWrite(Z_ENABLE_PIN, LOW);

  // Configure AccelStepper
  _x.setMaxSpeed(STEPPER_MAX_SPEED); _x.setAcceleration(STEPPER_ACCELERATION);
  _y.setMaxSpeed(STEPPER_MAX_SPEED); _y.setAcceleration(STEPPER_ACCELERATION);
  _z.setMaxSpeed(STEPPER_MAX_SPEED); _z.setAcceleration(STEPPER_ACCELERATION);

  // Endstop inputs
  pinMode(X_HOME_PIN, INPUT_PULLUP);
  pinMode(Y_HOME_PIN, INPUT_PULLUP);
  pinMode(Z_HOME_PIN, INPUT_PULLUP);
}

// ============================================================================
// Public interface
// ============================================================================

void SorterController::queueTarget(int targetID) {
  _queue.push(targetID);
}

void SorterController::triggerHoming() {
  Serial.println("Triggered Multi-Button Event -> Entering Homing Mode.");
  _queue.flush();
  _pipeline[0] = _pipeline[1] = _pipeline[2] = 0;
  _state = HOMING;
}

// ============================================================================
// update() — call every loop() iteration
// ============================================================================

void SorterController::update() {
  // Always keep steppers ticking
  _runAll();

  switch (_state) {

    case IDLE:
      if (!_queue.empty() ||
          _pipeline[0] != 0 || _pipeline[1] != 0 || _pipeline[2] != 0) {
        _state = NEW_BEAT_PREP;
      }
      break;

    case NEW_BEAT_PREP:
      Serial.println("\n--- NEW BEAT STARTED ---");
      _advancePipeline();
      _printPipelineState();

      // Issue relative moves — motors with MOTOR_NO_MOVE stay put.
      _applyMove(_x, _getTargetPos(0, _pipeline[0]));
      _applyMove(_y, _getTargetPos(1, _pipeline[1]));
      _applyMove(_z, _getTargetPos(2, _pipeline[2]));

      _state = MOVING_TO_ROUTE;
      break;

    case MOVING_TO_ROUTE:
      if (!_anyRunning()) {
        Serial.println("Reached Routing Positions. Waiting for slide...");
        _timerMark = millis();
        _state = SLIDING_WAIT;
      }
      break;

    case SLIDING_WAIT:
      if (millis() - _timerMark >= SLIDE_WAIT_MS) {
        // Wheel is 90-deg symmetric — motors stay at current position.
        Serial.println("Slide delay ended. Beat Finished.");
        _state = COMPLETED_BEAT;
      }
      break;

    case COMPLETED_BEAT:
      _state = IDLE;
      break;

    case HOMING:
      _doHomingStep();
      break;
  }
}

// ============================================================================
// Private helpers
// ============================================================================

long SorterController::_getTargetPos(int stage, int targetID) const {
  if (targetID == 0) return MOTOR_NO_MOVE; // empty slot

  switch (stage) {
    case 0: // Motor X — first sorter
      return (targetID == 1)
        ? MOTOR_X_DIR * ROTATE_LEFT_POS
        : MOTOR_X_DIR * ROTATE_RIGHT_POS;

    case 1: // Motor Y — second sorter
      if (targetID == 1) return MOTOR_NO_MOVE; // already exited at stage 0
      return (targetID == 2)
        ? MOTOR_Y_DIR * ROTATE_RIGHT_POS
        : MOTOR_Y_DIR * ROTATE_LEFT_POS;  // targets 3, 4

    case 2: // Motor Z — third sorter
      if (targetID == 1 || targetID == 2) return MOTOR_NO_MOVE; // already exited
      return (targetID == 3)
        ? MOTOR_Z_DIR * ROTATE_LEFT_POS
        : MOTOR_Z_DIR * ROTATE_RIGHT_POS; // target 4

    default:
      return MOTOR_NO_MOVE;
  }
}

void SorterController::_applyMove(AccelStepper& s, long target) {
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
  _pipeline[2] = _pipeline[1];
  _pipeline[1] = _pipeline[0];
  _pipeline[0] = _queue.pop();
}

void SorterController::_printPipelineState() const {
  Serial.print("Pipeline State: [M1: Target "); Serial.print(_pipeline[0]);
  Serial.print("] -> [M2: Target ");             Serial.print(_pipeline[1]);
  Serial.print("] -> [M3: Target ");             Serial.print(_pipeline[2]);
  Serial.println("]");
}

void SorterController::_doHomingStep() {
  if (!_homingInit) {
    Serial.println("Initiating Simultaneous Homing...");
    _xDone = _yDone = _zDone = false;
    _x.setSpeed(HOMING_CONSTANT_SPEED);
    _y.setSpeed(HOMING_CONSTANT_SPEED);
    _z.setSpeed(HOMING_CONSTANT_SPEED);
    _homingInit = true;
  }

  if (!_xDone) {
    if (digitalRead(X_HOME_PIN) == LOW) {
      _x.stop(); _x.setCurrentPosition(0);
      _xDone = true;
      Serial.println("X Homing Completed.");
    } else { _x.runSpeed(); }
  }

  if (!_yDone) {
    if (digitalRead(Y_HOME_PIN) == LOW) {
      _y.stop(); _y.setCurrentPosition(0);
      _yDone = true;
      Serial.println("Y Homing Completed.");
    } else { _y.runSpeed(); }
  }

  if (!_zDone) {
    if (digitalRead(Z_HOME_PIN) == LOW) {
      _z.stop(); _z.setCurrentPosition(0);
      _zDone = true;
      Serial.println("Z Homing Completed.");
    } else { _z.runSpeed(); }
  }

  if (_xDone && _yDone && _zDone) {
    Serial.println("--- ALL MOTORS HOMED & REZEROED ---");
    _homingInit = false;
    _state = IDLE;
  }
}
