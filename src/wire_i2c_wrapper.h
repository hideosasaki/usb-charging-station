// Adapts an Arduino-Pico TwoWire bus to the vendored SW35xx_lib
// I2CInterface contract. Owning the wrapper lets us pin the SDA/SCL
// GPIOs and the bus clock at begin() time, which is the part the
// upstream TwoWireWrapper does not do.

#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <stdint.h>

#include "I2CInterface.h"

class WireI2CWrapper : public I2CInterface {
 public:
  WireI2CWrapper(TwoWire& wire, uint8_t sda_pin, uint8_t scl_pin,
                 uint32_t clock_hz = 100000)
      : wire_(wire), sda_(sda_pin), scl_(scl_pin), clock_(clock_hz) {}

  void begin() override {
    wire_.setSDA(sda_);
    wire_.setSCL(scl_);
    wire_.begin();
    wire_.setClock(clock_);
  }

  void beginTransmission(uint8_t address) override {
    wire_.beginTransmission(address);
  }
  size_t write(uint8_t data) override { return wire_.write(data); }
  uint8_t endTransmission() override { return wire_.endTransmission(); }
  size_t requestFrom(uint8_t address, size_t quantity) override {
    return wire_.requestFrom(address, quantity);
  }
  int available() override { return wire_.available(); }
  int read() override { return wire_.read(); }

 private:
  TwoWire& wire_;
  uint8_t  sda_;
  uint8_t  scl_;
  uint32_t clock_;
};
