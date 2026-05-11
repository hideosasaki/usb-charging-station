// Shared test fixtures for PortReading / HistorySample. All four native
// test binaries (test_ring, test_analyzer, test_session, test_serial_cmd)
// used to define their own mk()/to_sample() pair; this header centralises
// the USB-C single-rail shape they all share so a field rename does not
// have to chase every copy.

#pragma once

#include "../../src/port_history.h"
#include "../../src/port_reader.h"

namespace test_support {

// A PortReading on the USB-C rail (no USB-A current, proto defaults to
// Pd30 — the analyzer tests assume the high-voltage path).
inline PortReading make_reading_c(uint16_t v_mV, uint16_t i_mA,
                                  bool attached = true, uint32_t t_ms = 0) {
  PortReading r{};
  r.t_ms   = t_ms;
  r.v_mV   = v_mV;
  r.i_c_mA = i_mA;
  r.proto  = Protocol::Pd30;
  r.err    = PortError::Ok;
  r.set_rail(Rail::UsbC, attached);
  return r;
}

// Direct HistorySample builder for ring-buffer tests that want to push
// arbitrary v/i values without going through a PortReading.
inline HistorySample make_sample_c(uint16_t v_mV, uint16_t i_mA,
                                   uint8_t proto = 0, bool attached = true) {
  HistorySample s{};
  s.v_mV   = v_mV;
  s.i_c_mA = i_mA;
  s.proto  = proto;
  s.flags  = attached ? kFlagAttached : 0;
  return s;
}

}  // namespace test_support
