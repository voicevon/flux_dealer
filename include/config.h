#ifndef CONFIG_H
#define CONFIG_H

#include <limits.h>

// ============================================================================
// Hardware: MKS BASE V1.6 / RAMPS 1.4 (ATmega2560)
// ============================================================================

// --- Microstep Configuration (must match MS1/MS2/MS3 jumpers on the board) ---
// A4988 / DRV8825: MS pins are set via hardware jumpers, NOT software.
// Verify: all 3 jumpers installed under each driver socket = 1/16 microstep.
#define MICROSTEP_RESOLUTION  16    // 1 | 2 | 4 | 8 | 16

// --- Stepper Geometry ---
#define MOTOR_FULL_STEPS      200   // steps/rev for a 1.8 deg/step motor
#define GEAR_RATIO            4     // 1:4 — motor turns 4x per wheel turn
// Steps to rotate the sorting wheel 90 degrees:
//   (full_steps * microstep * gear_ratio) / 4 = 3200
#define STEPS_PER_90DEG  (MOTOR_FULL_STEPS * MICROSTEP_RESOLUTION * GEAR_RATIO / 4)

// --- Motion Parameters ---
#define STEPPER_MAX_SPEED     6400.0f   // steps/s
#define STEPPER_ACCELERATION  6400.0f   // steps/s²
#define HOMING_CONSTANT_SPEED (-800.0f) // steps/s, negative = toward endstop

// --- Per-Motor Direction ---
// +1 = normal, -1 = reverse (flip when motor is physically mounted backwards)
#define MOTOR_X_DIR  (-1)   // Stage 1 motor
#define MOTOR_Y_DIR  (-1)   // Stage 2 motor
#define MOTOR_Z_DIR  (+1)   // Stage 3 motor

// --- Logical Positions (before direction multiplier) ---
#define POS_NEUTRAL       0L
#define ROTATE_LEFT_POS   (-(long)STEPS_PER_90DEG)
#define ROTATE_RIGHT_POS  ( (long)STEPS_PER_90DEG)

// --- Beat Timing ---
#define SLIDE_WAIT_MS  2000UL   // ms to wait for items to slide through

// --- Sentinel: motor should stay at its current position ---
// The wheel is 90-deg rotationally symmetric, so any stop angle == "neutral".
#define MOTOR_NO_MOVE  LONG_MIN

#endif // CONFIG_H
