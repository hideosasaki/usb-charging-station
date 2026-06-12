// Adapts the vendored pico-examples PIO I2C master (lib/PioI2C) to the
// SW35xx_lib I2CInterface contract, mirroring WireI2CWrapper. Used for
// the third SW3518: both hardware I2C controllers are already taken and
// the chip address is fixed, so the last bus runs on a PIO state machine.
//
// The PIO program requires SCL to be the GPIO directly after SDA and
// fixes the bus clock at 100 kHz. begin() claims the first free state
// machine on either PIO block; if none is free the wrapper stays inert
// and every transaction reports failure, which the port probe turns
// into a disabled port.

#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "I2CInterface.h"

extern "C" {
#include "pio_i2c.h"
}

class PioI2CWrapper : public I2CInterface {
 public:
  PioI2CWrapper(uint8_t sda_pin, uint8_t scl_pin)
      : sda_(sda_pin), scl_(scl_pin) {}

  void begin() override {
    if (sm_ >= 0) return;
    if (scl_ != sda_ + 1) return;  // wait-instruction pin mapping
    PIO blocks[2] = {pio0, pio1};
    for (PIO pio : blocks) {
      if (!pio_can_add_program(pio, &i2c_program)) continue;
      int sm = pio_claim_unused_sm(pio, false);
      if (sm < 0) continue;
      uint offset = pio_add_program(pio, &i2c_program);
      i2c_program_init(pio, (uint)sm, offset, sda_, scl_);
      pio_ = pio;
      sm_  = sm;
      break;
    }
  }

  void beginTransmission(uint8_t address) override {
    addr_   = address;
    tx_len_ = 0;
  }

  size_t write(uint8_t data) override {
    if (tx_len_ >= sizeof(tx_buf_)) return 0;
    tx_buf_[tx_len_++] = data;
    return 1;
  }

  // Wire-compatible result codes: 0 = success, 2 = NACK, 4 = other error.
  uint8_t endTransmission() override {
    if (sm_ < 0) return 4;
    int err = pio_i2c_write_blocking(pio_, (uint)sm_, addr_, tx_buf_, tx_len_);
    tx_len_ = 0;
    return (err == 0) ? 0 : 2;
  }

  size_t requestFrom(uint8_t address, size_t quantity) override {
    rx_len_ = 0;
    rx_pos_ = 0;
    if (sm_ < 0) return 0;
    if (quantity > sizeof(rx_buf_)) quantity = sizeof(rx_buf_);
    if (pio_i2c_read_blocking(pio_, (uint)sm_, address, rx_buf_, quantity) != 0)
      return 0;
    rx_len_ = quantity;
    return quantity;
  }

  int available() override { return (int)(rx_len_ - rx_pos_); }

  int read() override {
    return (rx_pos_ < rx_len_) ? rx_buf_[rx_pos_++] : -1;
  }

 private:
  uint8_t sda_;
  uint8_t scl_;
  PIO     pio_ = nullptr;
  int     sm_  = -1;

  uint8_t  addr_   = 0;
  uint8_t  tx_buf_[16];
  size_t   tx_len_ = 0;
  uint8_t  rx_buf_[16];
  size_t   rx_len_ = 0;
  size_t   rx_pos_ = 0;
};
