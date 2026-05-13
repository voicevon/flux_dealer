#ifndef INPUT_QUEUE_H
#define INPUT_QUEUE_H

#include <Arduino.h>

// ============================================================================
// InputQueue —— 固定大小的目标 ID 先进先出队列
//
// 用于缓存按钮按下产生的分拣目标 ID（1–4）。
// 值 0 为"空"哨兵：队列耗尽时 pop() 返回 0。
// ============================================================================
class InputQueue {
public:
  static const int CAPACITY = 10; // 最大缓存条目数

  InputQueue() : _head(0), _tail(0) {}

  // 压入一个目标 ID。队列满时静默丢弃，返回 false。
  bool push(int target) {
    int next = (_head + 1) % CAPACITY;
    if (next == _tail) return false; // 队列已满
    _buf[_head] = target;
    _head = next;
    Serial.print("已入队 -> 目标 "); Serial.println(target);
    return true;
  }

  // 弹出最早入队的目标 ID。队列为空时返回 0。
  int pop() {
    if (empty()) return 0;
    int val = _buf[_tail];
    _tail = (_tail + 1) % CAPACITY;
    return val;
  }

  // 队列是否为空
  bool empty() const { return _head == _tail; }

  // 丢弃所有待处理条目
  void flush() { _head = _tail = 0; }

private:
  int _buf[CAPACITY]; // 环形缓冲区
  int _head;          // 写指针（下一个写入位置）
  int _tail;          // 读指针（下一个弹出位置）
};

#endif // INPUT_QUEUE_H
