#include <unity.h>
#include "../../include/InputQueue.h"
#include "../../include/MotionPlanner.h"
#include "../../src/MotionPlanner.cpp"

// Define the ROUTING_TABLE for native test environment
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

void test_input_queue_push_pop() {
  InputQueue q;
  TEST_ASSERT_TRUE(q.empty());
  
  TEST_ASSERT_TRUE(q.push(3));
  TEST_ASSERT_TRUE(q.push(5));
  TEST_ASSERT_FALSE(q.empty());

  TEST_ASSERT_EQUAL(3, q.pop());
  TEST_ASSERT_EQUAL(5, q.pop());
  TEST_ASSERT_TRUE(q.empty());
  TEST_ASSERT_EQUAL(0, q.pop()); // empty sentinel
}

void test_input_queue_overflow() {
  InputQueue q;
  // CAPACITY is 10. Circle queue allows at most CAPACITY - 1 items.
  for (int i = 1; i <= 9; i++) {
    TEST_ASSERT_TRUE(q.push(i));
  }
  // 10th item should be rejected
  TEST_ASSERT_FALSE(q.push(10));
}

void test_motion_planner_routing() {
  MotionPlanner planner;
  int pipeline[NUM_MOTORS] = { 0 };
  int motorDirs[NUM_MOTORS] = { -1, -1, 1, 1, 1, 1, 1, 1 };

  // Case 1: Empty pipeline
  MotionCommand cmd = planner.planGroupMove(pipeline, motorDirs);
  TEST_ASSERT_FALSE(cmd.move_any);
  TEST_ASSERT_EQUAL(0, cmd.enable_mask);
  TEST_ASSERT_EQUAL(0, cmd.dir_state);

  // Case 2: Target 1 at stage 0 (should move M0)
  pipeline[0] = 1; // Target 1
  cmd = planner.planGroupMove(pipeline, motorDirs);
  TEST_ASSERT_TRUE(cmd.move_any);
  TEST_ASSERT_EQUAL(0x01, cmd.enable_mask); // M0 active
  // ROUTING_TABLE[1][0] = -1. motorDirs[0] = -1. dir = (-1)*(-1) = 1 (positive direction). So dir_state bit 0 should be 1.
  TEST_ASSERT_EQUAL(0x01, cmd.dir_state); 

  // Case 3: Target 2 at stage 0, Target 1 at stage 1
  pipeline[0] = 2; // Target 2 (routing table at stage 0 is +1 (forward))
  pipeline[1] = 1; // Target 1 (routing table at stage 1 is 0 (no move))
  cmd = planner.planGroupMove(pipeline, motorDirs);
  // M0 active. M1 target 1 at stage 1: ROUTING_TABLE[1][1] = 0 -> no move.
  TEST_ASSERT_TRUE(cmd.move_any);
  TEST_ASSERT_EQUAL(0x01, cmd.enable_mask); 
  // ROUTING_TABLE[2][0] = 1. motorDirs[0] = -1. dir = (1)*(-1) = -1. dir_state bit 0 should be 0.
  TEST_ASSERT_EQUAL(0x00, cmd.dir_state);
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_input_queue_push_pop);
  RUN_TEST(test_input_queue_overflow);
  RUN_TEST(test_motion_planner_routing);
  return UNITY_END();
}
