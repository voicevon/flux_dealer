#include "BleManager.h"
#include "Logger.h"
#include <Arduino.h>

class ServerCallbacks : public BLEServerCallbacks {
  BleManager* _manager;
public:
  ServerCallbacks(BleManager* manager) : _manager(manager) {}
  void onConnect(BLEServer* pServer) override {
    LOG_I("蓝牙已连接");
    _manager->_deviceConnected = true;
  }
  void onDisconnect(BLEServer* pServer) override {
    LOG_I("蓝牙断开连接，准备重新广播...");
    _manager->_deviceConnected = false;
    pServer->startAdvertising();
  }
};

class TargetCallbacks : public BLECharacteristicCallbacks {
  SorterController* _sorter;
public:
  TargetCallbacks(SorterController* sorter) : _sorter(sorter) {}
  void onWrite(BLECharacteristic *pCharacteristic) override {
    std::string value = pCharacteristic->getValue();
    if (value.length() > 0) {
      // 假设手机端发送的数字是以单个字节形式 (如 0x01 表示 Target 1)
      // 如果发的是 ASCII 字符 (如 '1')，这里需要转换
      int targetID = value[0];
      if (targetID >= '0' && targetID <= '9') {
        targetID = targetID - '0';
      }
      LOG_I("BLE 接收到目标物 ID: %d", targetID);
      
      // 推入队列
      _sorter->queueTarget(targetID);
    }
  }
};

class CmdCallbacks : public BLECharacteristicCallbacks {
  SorterController* _sorter;
public:
  CmdCallbacks(SorterController* sorter) : _sorter(sorter) {}
  void onWrite(BLECharacteristic *pCharacteristic) override {
    std::string value = pCharacteristic->getValue();
    if (value.length() > 0) {
      uint8_t cmd = value[0];
      LOG_I("BLE 接收到系统指令: 0x%02X", cmd);
      if (cmd == 0x01) {
        _sorter->clearError();
      } else if (cmd == 0x02) {
        _sorter->triggerHoming();
      }
    }
  }
};

BleManager::BleManager() 
  : _sorter(nullptr), _pServer(nullptr), 
    _pTargetChar(nullptr), _pStatusChar(nullptr), 
    _pErrorChar(nullptr), _pCmdChar(nullptr), 
    _deviceConnected(false) 
{}

void BleManager::begin(SorterController* sorter) {
  _sorter = sorter;

  BLEDevice::init("Sorter_Controller");
  _pServer = BLEDevice::createServer();
  _pServer->setCallbacks(new ServerCallbacks(this));

  BLEService *pService = _pServer->createService(SORTER_SERVICE_UUID);

  // 1. 接收 Target ID 特征
  _pTargetChar = pService->createCharacteristic(
                      TARGET_CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_WRITE
                    );
  _pTargetChar->setCallbacks(new TargetCallbacks(_sorter));

  // 2. 广播状态特征
  _pStatusChar = pService->createCharacteristic(
                      STATUS_CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
  _pStatusChar->addDescriptor(new BLE2902());

  // 3. 广播错误特征
  _pErrorChar = pService->createCharacteristic(
                      ERROR_CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
  _pErrorChar->addDescriptor(new BLE2902());

  // 4. 接收指令特征
  _pCmdChar = pService->createCharacteristic(
                      COMMAND_CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_WRITE
                    );
  _pCmdChar->setCallbacks(new CmdCallbacks(_sorter));

  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SORTER_SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

  LOG_I("BLE 服务已启动，包含目标/状态/错误/指令特征，等待 phone_sorter 连接...");
}

void BleManager::updateStatus(SorterController::State state) {
  if (!_deviceConnected) return;

  uint8_t stateVal = static_cast<uint8_t>(state);
  _pStatusChar->setValue(&stateVal, 1);
  _pStatusChar->notify();
}

void BleManager::updateError(SorterController::ErrorCode errorCode) {
  if (!_deviceConnected) return;

  uint8_t errorVal = static_cast<uint8_t>(errorCode);
  _pErrorChar->setValue(&errorVal, 1);
  _pErrorChar->notify();
}

bool BleManager::isConnected() const {
  return _deviceConnected;
}
