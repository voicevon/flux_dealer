#include "hal/SensorInput.h"

SensorInput::SensorInput(uint8_t pin, unsigned long debounceMs)
  : _pin(pin), _debounceMs(debounceMs), 
    _currentState(false), _lastState(false), _risingEdge(false),
    _lastChangeTime(0), _rawState(false) 
{
}

void SensorInput::begin() {
  pinMode(_pin, INPUT_PULLUP);
  _rawState = (digitalRead(_pin) == LOW);
  _currentState = _rawState;
  _lastState = _currentState;
}

void SensorInput::update() {
  bool reading = (digitalRead(_pin) == LOW); // LOW 表示遮挡(触发)
  
  if (reading != _rawState) {
    _lastChangeTime = millis();
    _rawState = reading;
  }
  
  _risingEdge = false;
  
  if ((millis() - _lastChangeTime) > _debounceMs) {
    if (reading != _currentState) {
      _currentState = reading;
    }
  }
  
  if (_currentState == true && _lastState == false) {
    _risingEdge = true;
  }
  
  _lastState = _currentState;
}

bool SensorInput::isTriggered() const {
  return _currentState;
}

bool SensorInput::isRisingEdge() const {
  return _risingEdge;
}
