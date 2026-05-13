#ifndef BUTTON_SCANNER_H
#define BUTTON_SCANNER_H

#include <Arduino.h>
#include "pins.h"

// ============================================================================
// ButtonScanner
//
// Debounced scanner for the 4 target-selection buttons.
// Call begin() once in setup(), then scan() every loop iteration.
//
// Events are delivered via two callbacks set at construction time:
//   onTarget(int targetID)  — called once per button press (1..4)
//   onHomingTriggered()     — called when ≥2 buttons are pressed simultaneously
// ============================================================================
class ButtonScanner {
public:
  // Callback types
  using TargetCb = void (*)(int targetID);
  using HomingCb = void (*)();

  // -------------------------------------------------------------------------
  ButtonScanner(TargetCb onTarget, HomingCb onHoming)
    : _onTarget(onTarget), _onHoming(onHoming),
      _homingFired(false), _lastScan(0)
  {
    for (int i = 0; i < NUM_BTNS; i++) _state[i] = false;
  }

  // Call once in setup() to configure pin modes.
  void begin() {
    pinMode(BTN_TARGET_1_PIN, INPUT_PULLUP);
    pinMode(BTN_TARGET_2_PIN, INPUT_PULLUP);
    pinMode(BTN_TARGET_3_PIN, INPUT_PULLUP);
    pinMode(BTN_TARGET_4_PIN, INPUT_PULLUP);
  }

  // Call every loop() — honours 50 ms debounce interval internally.
  void scan() {
    if (millis() - _lastScan < DEBOUNCE_MS) return;
    _lastScan = millis();

    const uint8_t pins[NUM_BTNS] = {
      BTN_TARGET_1_PIN, BTN_TARGET_2_PIN,
      BTN_TARGET_3_PIN, BTN_TARGET_4_PIN
    };

    bool now[NUM_BTNS];
    int  pressCount = 0;

    for (int i = 0; i < NUM_BTNS; i++) {
      now[i] = (digitalRead(pins[i]) == LOW); // LOW = pressed (INPUT_PULLUP)
      if (now[i]) pressCount++;
    }

    // Multi-button → homing (fire once per gesture, reset when released)
    if (pressCount >= 2) {
      if (!_homingFired) {
        _homingFired = true;
        if (_onHoming) _onHoming();
      }
    } else {
      _homingFired = false; // allow re-trigger after release
    }

    // Single-button rising-edge → queue target (only when not in homing gesture)
    if (pressCount < 2) {
      for (int i = 0; i < NUM_BTNS; i++) {
        if (now[i] && !_state[i] && _onTarget) {
          _logTransition(pins[i], i + 1, true);
          _onTarget(i + 1);
        } else if (!now[i] && _state[i]) {
          _logTransition(pins[i], i + 1, false);
        }
      }
    }

    // Commit new state
    for (int i = 0; i < NUM_BTNS; i++) _state[i] = now[i];
  }

private:
  static const int  NUM_BTNS   = 4;
  static const unsigned long DEBOUNCE_MS = 50;

  TargetCb      _onTarget;
  HomingCb      _onHoming;
  bool          _state[NUM_BTNS];
  bool          _homingFired;
  unsigned long _lastScan;

  static void _logTransition(uint8_t pin, int btnNum, bool pressed) {
    Serial.print("[BTN] Pin "); Serial.print(pin);
    Serial.print(" (BTN "); Serial.print(btnNum); Serial.print(") -> ");
    Serial.println(pressed ? "LOW (Pressed)" : "HIGH (Released)");
  }
};

#endif // BUTTON_SCANNER_H
