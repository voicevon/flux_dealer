#include "apps/AppProduction.h"
#include "FluxDealer.h"
#include "Logger.h"
#include <Arduino.h>

AppProduction::AppProduction(FluxDealer& controller) : _controller(controller) {}

void AppProduction::begin() {
  LOG_I("AppProduction (正常生产模式) 启动");
}

void AppProduction::end() {
  LOG_I("AppProduction (正常生产模式) 结束");
}

void AppProduction::update() {
  switch (_controller._state) {
    case FluxDealer::IDLE: {
      _controller._entranceSensor.update();
      if (_controller._entranceSensor.isRisingEdge()) {
        _controller._state = FluxDealer::NEW_BEAT_PREP;
      }
      break;
    }

    case FluxDealer::NEW_BEAT_PREP:
      LOG_I("--- 新节拍开始 ---");
      _controller._advancePipeline();
      _controller._printPipelineState();

      // 规划并下发组运动
      {
        MotionCommand cmd = _controller._planner.planGroupMove(_controller._pipeline, _controller._motorDirs);
        _controller._motorHardware.setEnableMask(cmd.enable_mask);
        _controller._spiBus.transfer(cmd.dir_state);

        if (cmd.move_any) {
          _controller._motorHardware.startMove(cmd.steps);
        }
      }

      _controller._state = FluxDealer::MOVING_TO_ROUTE;
      break;

    case FluxDealer::MOVING_TO_ROUTE:
      if (!_controller._motorHardware.isMoving()) {
        LOG_D("已到达分拣位置，等待物品滑行...");
        _controller._motorHardware.setCurrentPosition(0);
        _controller._timerMark = millis();
        _controller._state = FluxDealer::SLIDING_WAIT;
      }
      break;

    case FluxDealer::SLIDING_WAIT:
      if (millis() - _controller._timerMark >= SLIDE_WAIT_MS) {
        LOG_D("滑行等待结束，本节拍完成");
        _controller._state = FluxDealer::COMPLETED_BEAT;
      }
      break;

    case FluxDealer::COMPLETED_BEAT:
      _controller._state = FluxDealer::IDLE;
      break;

    case FluxDealer::HOMING:
      if (!_controller._homing.update()) {
        if (_controller._homing.hasError()) {
          _controller._errorCode = FluxDealer::ERR_HOMING_TIMEOUT;
          _controller._state = FluxDealer::ERROR_STATE;
        } else if (_controller._homing.isHomingDone()) {
          _controller._state = FluxDealer::IDLE;
        }
      }
      break;

    case FluxDealer::ERROR_STATE:
      // 等待外部复位
      break;
    
    default:
      break;
  }
}
