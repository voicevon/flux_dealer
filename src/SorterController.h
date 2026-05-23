#ifndef SORTER_CONTROLLER_H
#define SORTER_CONTROLLER_H

#include <AccelStepper.h>
#include "config.h"
#include "InputQueue.h"
#include "hal/MotorHardware.h"
#include "hal/ShiftRegisterBus.h"
#include "hal/SensorInput.h"
#include "MotionPlanner.h"
#include "HomingController.h"

// ============================================================================
// SorterController —— 分拣机核心控制器
//
// 负责管理三级物品流水线与节拍同步状态机。
// 在 setup() 中调用 begin() 完成硬件初始化，
// 在每次 loop() 中调用 update() 驱动状态机与电机。
// 通过 queueTarget() 投入待分拣目标，通过 triggerHoming() 触发归零。
// ============================================================================
class AppBase;
class AppProduction;
class AppMotorDiag;
class AppHallDiag;

class SorterController {
public:
  // ---- 公开状态枚举（可供主程序读取）---------------------------------------
  enum State {
    IDLE,             // 空闲，等待任务
    NEW_BEAT_PREP,    // 新节拍准备：推进流水线、下发电机指令
    MOVING_TO_ROUTE,  // 电机运动中，等待到位
    SLIDING_WAIT,     // 物品滑行等待（自然重力滑道）
    COMPLETED_BEAT,   // 本节拍结束，准备回到空闲
    HOMING,           // 归零校准中
    ERROR_STATE,      // 硬件或运行异常状态
    DIAG_MOTOR,       // 诊断电机模式
    DIAG_HALL         // 诊断霍尔模式
  };

  enum ErrorCode {
    ERR_NONE = 0,
    ERR_HOMING_TIMEOUT = 1,
    ERR_SENSOR_FAULT = 2,
    ERR_QUEUE_OVERFLOW = 3,
    ERR_BLE_DISCONNECT = 4
  };

  // -------------------------------------------------------------------------
  SorterController(AccelStepper& sharedStepper);
  ~SorterController();

  // 在 setup() 中调用一次——配置电机速度/加速度及限位开关引脚。
  void begin();

  // 在每次 loop() 中调用——驱动状态机与步进电机。
  void update();

  // 将排序目标（1–8）加入队列。
  void queueTarget(int targetID);

  // 中止当前任务，进入归零校准模式。
  void triggerHoming();

  // 切换当前运行的 APP 及其生命周期
  void switchApp(State newState);

  // 处理短按事件以进行诊断行为切换
  void handleShortPress();

  State getState() const { return _state; }
  ErrorCode getErrorCode() const { return _errorCode; }
  
  // 用于软复位：清除错误状态并重新触发归零
  void clearError();

private:
  friend class AppProduction;
  friend class AppMotorDiag;
  friend class AppHallDiag;

  // ---- HAL 对象 ------------------------------------------------------------
  MotorHardware _motorHardware;
  ShiftRegisterBus _spiBus;
  SensorInput _entranceSensor;

  // ---- 逻辑与控制层对象 ----------------------------------------------------
  MotionPlanner _planner;
  HomingController _homing;

  // ---- 状态机 --------------------------------------------------------------
  State         _state;
  unsigned long _timerMark;  // 滑行计时起始时间戳
  ErrorCode     _errorCode;  // 当前错误状态码

  // ---- 多级物品流水线 -------------------------------------------------------
  // _pipeline[0] = 当前在第一级电机位置的物品（最新入队）
  // ... _pipeline[NUM_MOTORS-1]
  // 值 0 = 空槽；1及以上 = 目标分拣口 ID。
  int _pipeline[NUM_MOTORS];

  // ---- 输入缓冲 ------------------------------------------------------------
  InputQueue _queue;

  // ---- 硬件配置与状态 -----------------------------------------------------
  int _motorDirs[NUM_MOTORS];

  // ---- APP 实例指针 --------------------------------------------------------
  AppProduction* _appProduction;
  AppMotorDiag*  _appMotorDiag;
  AppHallDiag*   _appHallDiag;
  AppBase*       _activeApp;

  // ---- 私有辅助函数 --------------------------------------------------------

  void _advancePipeline();     // 流水线整体前移一格，弹出队首目标
  void _printPipelineState() const; // 串口打印当前流水线状态
};

#endif // SORTER_CONTROLLER_H
