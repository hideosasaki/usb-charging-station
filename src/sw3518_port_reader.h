// PortReader implementation backed by an SW3518 chip on an I2C bus.
// The I2C backend is injected so Phase 4 can swap a PIO-I2C bus in
// without touching the reader.

#pragma once

#include <stdint.h>

#include "I2CInterface.h"
#include "SW35xx_lib.h"
#include "port_reader.h"

class Sw3518PortReader : public PortReader {
 public:
  Sw3518PortReader(uint8_t idx, I2CInterface& bus);

  bool        begin() override;
  PortReading read(uint32_t now_ms) override;
  uint8_t     index() const override { return idx_; }

 private:
  uint8_t                 idx_;
  I2CInterface&           bus_;
  SW35xx_lib::SW35xx      chip_;
  bool                    probed_ok_ = false;
  uint32_t                last_ok_ms_ = 0;
};
