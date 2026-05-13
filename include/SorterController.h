#ifndef SORTER_CONTROLLER_H
#define SORTER_CONTROLLER_H

#include <AccelStepper.h>
#include "config.h"
#include "InputQueue.h"

// ============================================================================
// SorterController
//
// Owns the 3-stage item pipeline and the beat-synchronised state machine.
// Call begin() once in setup(), then update() every loop iteration.
// Feed incoming sort targets with queueTarget(); trigger re-homing with
// triggerHoming().
// ============================================================================
class SorterController {
public:
  // ----- Public state enum (readable by the main sketch) -------------------
  enum State {
    IDLE,
    NEW_BEAT_PREP,
    MOVING_TO_ROUTE,
    SLIDING_WAIT,
    COMPLETED_BEAT,
    HOMING
  };

  // -------------------------------------------------------------------------
  SorterController(AccelStepper& motorX,
                   AccelStepper& motorY,
                   AccelStepper& motorZ);

  // Call once in setup() — configures motor speed/accel and endstop pins.
  void begin();

  // Call every loop() iteration — drives the state machine and steppers.
  void update();

  // Enqueue a sort target (1–4). Safe to call from any context (e.g. ISR-safe
  // because it only does an atomic array write on AVR).
  void queueTarget(int targetID);

  // Abort current work and enter calibration homing mode.
  void triggerHoming();

  State getState() const { return _state; }

private:
  // ---- Motor references ---------------------------------------------------
  AccelStepper& _x;
  AccelStepper& _y;
  AccelStepper& _z;

  // ---- State machine ------------------------------------------------------
  State         _state;
  unsigned long _timerMark;

  // ---- 3-stage item pipeline ----------------------------------------------
  // _pipeline[0] = item at Motor X (newest entry)
  // _pipeline[1] = item at Motor Y
  // _pipeline[2] = item at Motor Z (oldest, about to exit)
  // Value 0 = empty slot; 1-4 = destination ID.
  int _pipeline[3];

  // ---- Input buffer -------------------------------------------------------
  InputQueue _queue;

  // ---- Homing sub-state (replaces old static locals) ----------------------
  bool _homingInit;
  bool _xDone, _yDone, _zDone;

  // ---- Private helpers ----------------------------------------------------

  // Returns the desired absolute step-position for a given stage+targetID,
  // or MOTOR_NO_MOVE when the motor should not move (item already exited, or
  // slot empty — wheel is 90-deg symmetric so any stop == neutral).
  long _getTargetPos(int stage, int targetID) const;

  // Issue a relative move only when a real target position is provided.
  void _applyMove(AccelStepper& s, long target);

  void _runAll();
  bool _anyRunning() const;
  void _doHomingStep();

  void _advancePipeline();
  void _printPipelineState() const;
};

#endif // SORTER_CONTROLLER_H
