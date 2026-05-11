// Abstract per-port reader contract. The Controller pulls samples from a
// PortReader once per second and feeds them into the model layer. Real
// implementations talk to SW3518 over I2C; the mock generates a scenario.

#pragma once

#include <stdint.h>

enum class Protocol : uint8_t {
  None,
  Std5V,
  Qc20,
  Qc30,
  Pd20,
  Pd30,
  Pd31Pps,
};

enum class PortError : uint8_t {
  Ok,
  NotPresent,
  I2cTimeout,
  I2cNack,
  Stale,
};

struct PortReading {
  uint32_t  t_ms;
  uint16_t  v_mV;
  uint16_t  i_mA;
  Protocol  proto;
  PortError err;
  bool      attached;
};

class PortReader {
 public:
  virtual ~PortReader() = default;
  virtual bool        begin() = 0;
  virtual PortReading read(uint32_t now_ms) = 0;
  virtual uint8_t     index() const = 0;
};

// Factory implemented by exactly one of mock_port_reader.cpp or
// sw3518_port_reader.cpp, selected at compile time via USE_MOCK_PORTS.
PortReader* make_port_reader(uint8_t idx);

inline const char* protocol_name(Protocol p) {
  switch (p) {
    case Protocol::None:    return "--";
    case Protocol::Std5V:   return "5V";
    case Protocol::Qc20:    return "QC2.0";
    case Protocol::Qc30:    return "QC3.0";
    case Protocol::Pd20:    return "PD2.0";
    case Protocol::Pd30:    return "PD3.0";
    case Protocol::Pd31Pps: return "PPS";
  }
  return "?";
}
