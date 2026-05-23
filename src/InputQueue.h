#ifndef INPUT_QUEUE_H
#define INPUT_QUEUE_H

#ifdef ARDUINO
#include <Arduino.h>
#include "Logger.h"
#else
#include <stdint.h>
#define portMUX_TYPE int
#define portMUX_INITIALIZE(x)
#define portENTER_CRITICAL(x)
#define portEXIT_CRITICAL(x)
#define LOG_W(...)
#define LOG_D(...)
#endif

// ============================================================================
// InputQueue —— 固定大小的目标 ID 先进先出队列
//
// 用于缓存分拣目标 ID（1–8）。
// 值 0 为"空"哨兵：队列耗尽时 pop() 返回 0。
//
// 线程安全说明：
//   BLE 回调在独立任务线程中执行 push()，主循环执行 pop()。
//   使用 portMUX 临界区保护共享状态。
// ============================================================================
class InputQueue {
public:
  static const int CAPACITY = 10; // 最大缓存条目数

  InputQueue() : _head(0), _tail(0) {
    portMUX_INITIALIZE(&_mux);
  }

  // 压入一个目标 ID。队列满时静默丢弃，返回 false。
  bool push(int target) {
    portENTER_CRITICAL(&_mux);
    int next = (_head + 1) % CAPACITY;
    if (next == _tail) {
      portEXIT_CRITICAL(&_mux);
      LOG_W("队列已满，丢弃目标 %d", target);
      return false;
    }
    _buf[_head] = target;
    _head = next;
    portEXIT_CRITICAL(&_mux);
    LOG_D("已入队 -> 目标 %d", target);
    return true;
  }

  // 弹出最早入队的目标 ID。队列为空时返回 0。
  int pop() {
    portENTER_CRITICAL(&_mux);
    if (_head == _tail) {
      portEXIT_CRITICAL(&_mux);
      return 0;
    }
    int val = _buf[_tail];
    _tail = (_tail + 1) % CAPACITY;
    portEXIT_CRITICAL(&_mux);
    return val;
  }

  // 队列是否为空
  bool empty() const {
    portENTER_CRITICAL(&_mux);
    bool e = (_head == _tail);
    portEXIT_CRITICAL(&_mux);
    return e;
  }

  // 丢弃所有待处理条目
  void flush() {
    portENTER_CRITICAL(&_mux);
    _head = _tail = 0;
    portEXIT_CRITICAL(&_mux);
  }

private:
  int _buf[CAPACITY]; // 环形缓冲区
  volatile int _head; // 写指针（下一个写入位置）
  volatile int _tail; // 读指针（下一个弹出位置）
  mutable portMUX_TYPE _mux; // 自旋锁
};

#endif // INPUT_QUEUE_H
