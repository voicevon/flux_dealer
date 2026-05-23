#ifndef APP_HALL_DIAG_H
#define APP_HALL_DIAG_H

#include "apps/AppBase.h"

class SorterController;

class AppHallDiag : public AppBase {
public:
  AppHallDiag(SorterController& controller);
  void begin() override;
  void update() override;
  void end() override;

private:
  SorterController& _controller;
  unsigned long _lastPrintTime;
  uint8_t _lastHomeState;
};

#endif // APP_HALL_DIAG_H
