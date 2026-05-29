#ifndef APP_PRODUCTION_H
#define APP_PRODUCTION_H

#include "apps/AppBase.h"

class FluxDealer;

class AppProduction : public AppBase {
public:
  AppProduction(FluxDealer& controller);
  void begin() override;
  void update() override;
  void end() override;

private:
  FluxDealer& _controller;
};

#endif // APP_PRODUCTION_H
