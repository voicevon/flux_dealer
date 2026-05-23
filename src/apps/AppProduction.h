#ifndef APP_PRODUCTION_H
#define APP_PRODUCTION_H

#include "apps/AppBase.h"

class SorterController;

class AppProduction : public AppBase {
public:
  AppProduction(SorterController& controller);
  void begin() override;
  void update() override;
  void end() override;

private:
  SorterController& _controller;
};

#endif // APP_PRODUCTION_H
