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

// A SW3518 port has two physical rails: USB-C and USB-A. Vbus and the
// negotiated protocol are shared (the chip routes one voltage rail to
// either or both connectors). Currents are per-rail.
enum class Rail : uint8_t { UsbC = 0, UsbA = 1 };

struct PortReading {
  uint32_t  t_ms;
  uint16_t  v_mV;       // shared Vbus across both rails
  uint16_t  i_c_mA;     // USB-C rail current
  uint16_t  i_a_mA;     // USB-A rail current
  Protocol  proto;      // applies to the USB-C rail; USB-A is implicit Std5V
  PortError err;
  uint8_t   rail_mask;  // bit0=C attached, bit1=A attached

  bool has(Rail rail) const { return (rail_mask & bit_for(rail)) != 0; }
  bool has_c()        const { return has(Rail::UsbC); }
  bool has_a()        const { return has(Rail::UsbA); }
  bool attached()     const { return rail_mask != 0; }

  uint16_t i_mA(Rail rail) const {
    return rail == Rail::UsbC ? i_c_mA : i_a_mA;
  }
  // Vbus is shared, so port power equals V * (Ic + Ia) regardless of
  // which connector is delivering current.
  uint16_t total_i_mA() const {
    return static_cast<uint16_t>(
        static_cast<uint32_t>(i_c_mA) + static_cast<uint32_t>(i_a_mA));
  }

  void set_rail(Rail rail, bool on) {
    uint8_t bit = bit_for(rail);
    rail_mask = on ? (rail_mask | bit) : (rail_mask & ~bit);
  }
  void clear_rails() { rail_mask = 0; }

 private:
  static constexpr uint8_t bit_for(Rail r) {
    return r == Rail::UsbC ? 0x1 : 0x2;
  }
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
