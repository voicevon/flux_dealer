#ifndef MOTION_PLANNER_H
#define MOTION_PLANNER_H

#ifdef ARDUINO
#include <Arduino.h>
#else
#include <stdint.h>
#endif
#include "config.h"

struct MotionCommand {
  uint8_t dir_state;     // 74HC595 方向位图
  uint8_t enable_mask;   // 脉冲屏蔽掩码，1 表示放行 (使能)，0 表示屏蔽
  bool move_any;         // 本节拍是否有电机需要移动
  long steps;            // 统一步数 (STEPS_PER_90DEG)
};

class MotionPlanner {
public:
  MotionPlanner();

  // 根据流水线中各级的目标和换向数组，计算出该节拍的组运动指令
  MotionCommand planGroupMove(const int pipeline[NUM_MOTORS], const int motorDirs[NUM_MOTORS]) const;

private:
  // 计算单轴的目标绝对步数位置
  long _getTargetPos(int stage, int targetID, int motorDir) const;
};

#endif // MOTION_PLANNER_H
