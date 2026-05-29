#ifndef APP_HALL_DIAG_H
#define APP_HALL_DIAG_H

#include "apps/AppBase.h"

class FluxDealer;

class AppHallDiag : public AppBase {
public:
  AppHallDiag(FluxDealer& controller);
  void begin() override;
  void update() override;
  void end() override;

private:
  FluxDealer& _controller;
  unsigned long _lastPrintTime;
  uint8_t _lastHomeState;
};

#endif // APP_HALL_DIAG_H
