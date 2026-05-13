#include <Arduino.h>
#include <AccelStepper.h>
#include "pins.h"
#include "SorterController.h"
#include "ButtonScanner.h"

// ============================================================================
// Hardware: MKS BASE V1.6 (ATmega2560)
// Three stepper drivers in DRIVER (step/dir) mode.
// ============================================================================
AccelStepper stepperX(AccelStepper::DRIVER, X_STEP_PIN, X_DIR_PIN);
AccelStepper stepperY(AccelStepper::DRIVER, Y_STEP_PIN, Y_DIR_PIN);
AccelStepper stepperZ(AccelStepper::DRIVER, Z_STEP_PIN, Z_DIR_PIN);

SorterController sorter(stepperX, stepperY, stepperZ);

// ============================================================================
// Button event callbacks (free functions passed to ButtonScanner)
// ============================================================================
void onTargetPressed(int targetID) { sorter.queueTarget(targetID); }
void onHomingTriggered()           { sorter.triggerHoming();        }

ButtonScanner buttons(onTargetPressed, onHomingTriggered);

// ============================================================================
// setup / loop
// ============================================================================
void setup() {
  pinMode(LED_PIN, OUTPUT);

  Serial.begin(115200);
  while (!Serial) {}

  Serial.println("--- Sorter Mini Controller ---");
  Serial.println("Hardware: MKS BASE V1.6 (ATmega2560)");

  sorter.begin();
  buttons.begin();

  Serial.println("Initialisation complete. Ready.");
}

void loop() {
  // --- Heartbeat LED (1 Hz) ---
  static unsigned long lastBeat = 0;
  if (millis() - lastBeat >= 1000) {
    lastBeat = millis();
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }

  buttons.scan();   // Debounce & dispatch button events
  sorter.update();  // Drive state machine & steppers
}
