#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "SorterController.h"

// 蓝牙服务与特征 UUID (可以使用预定义的标准或自定义)
#define SORTER_SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define TARGET_CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8" // 手机端向机器发送目标 ID
#define STATUS_CHARACTERISTIC_UUID "1c95d5e3-d8f7-413a-bf3d-7a2e5d7be87e" // 机器向手机端广播当前状态
#define ERROR_CHARACTERISTIC_UUID  "2c95d5e3-d8f7-413a-bf3d-7a2e5d7be87e" // 机器向手机端广播当前错误码 (Notify)
#define COMMAND_CHARACTERISTIC_UUID "3c95d5e3-d8f7-413a-bf3d-7a2e5d7be87e" // 手机端向机器下发控制指令

class BleManager {
public:
  BleManager();
  void begin(SorterController* sorter);
  void updateStatus(SorterController::State state);
  void updateError(SorterController::ErrorCode errorCode);
  bool isConnected() const;

private:
  SorterController* _sorter;
  BLEServer* _pServer;
  BLECharacteristic* _pTargetChar;
  BLECharacteristic* _pStatusChar;
  BLECharacteristic* _pErrorChar;
  BLECharacteristic* _pCmdChar;
  bool _deviceConnected;
  
  // 回调类声明需要成为友元或内部类，为了简单，在 cpp 文件中实现一个全局的或局部的回调即可
  friend class TargetCallbacks;
  friend class ServerCallbacks;
  friend class CmdCallbacks;
};

#endif // BLE_MANAGER_H
