#ifndef INPUT_QUEUE_H
#define INPUT_QUEUE_H

#include <Arduino.h>

// ============================================================================
// Simple fixed-size FIFO queue for incoming button-press target IDs.
// Value 0 is the "empty" sentinel returned when the queue is drained.
// ============================================================================
class InputQueue {
public:
  static const int CAPACITY = 10;

  InputQueue() : _head(0), _tail(0) {}

  // Push a target ID. Returns false and drops silently if the queue is full.
  bool push(int target) {
    int next = (_head + 1) % CAPACITY;
    if (next == _tail) return false; // full
    _buf[_head] = target;
    _head = next;
    Serial.print("Queued Task -> Target "); Serial.println(target);
    return true;
  }

  // Pop and return the oldest item. Returns 0 if the queue is empty.
  int pop() {
    if (empty()) return 0;
    int val = _buf[_tail];
    _tail = (_tail + 1) % CAPACITY;
    return val;
  }

  bool empty() const { return _head == _tail; }

  // Discard all pending items.
  void flush() { _head = _tail = 0; }

private:
  int _buf[CAPACITY];
  int _head;
  int _tail;
};

#endif // INPUT_QUEUE_H
