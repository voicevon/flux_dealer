#include "MotionPlanner.h"

MotionPlanner::MotionPlanner() {
}

MotionCommand MotionPlanner::planGroupMove(const int pipeline[NUM_MOTORS], const int motorDirs[NUM_MOTORS]) const {
  MotionCommand cmd;
  cmd.dir_state = 0;
  cmd.enable_mask = 0;
  cmd.move_any = false;
  cmd.steps = STEPS_PER_90DEG;

  for (int i = 0; i < NUM_MOTORS; i++) {
    long tx = _getTargetPos(i, pipeline[i], motorDirs[i]);
    if (tx != MOTOR_NO_MOVE) {
      cmd.move_any = true;
      cmd.enable_mask |= (1 << i); // 1 = 放行
      if (tx > 0) {
        cmd.dir_state |= (1 << i); // 1 = 正转方向
      }
    }
  }

  return cmd;
}

long MotionPlanner::_getTargetPos(int stage, int targetID, int motorDir) const {
  if (targetID < 0 || targetID > MAX_TARGETS) return MOTOR_NO_MOVE;
  
  int8_t action = ROUTING_TABLE[targetID][stage];
  if (action == 0) return MOTOR_NO_MOVE;

  int dir = action * motorDir;
  return (dir > 0) ? ROTATE_RIGHT_POS : ROTATE_LEFT_POS; 
}
