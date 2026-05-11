// Live PortReading -> ring-buffer HistorySample. Same function the
// Controller calls every tick (src/app.cpp) and tests use to seed
// PortHistory; keeping one definition avoids the two copies drifting
// when fields move around.

#pragma once

#include "port_history.h"
#include "port_reader.h"

inline HistorySample to_sample(const PortReading& r) {
  HistorySample s{};
  s.v_mV   = r.v_mV;
  s.i_c_mA = r.i_c_mA;
  s.i_a_mA = r.i_a_mA;
  s.proto  = (uint8_t)r.proto;
  s.flags  = (r.attached() ? kFlagAttached : 0) |
             (r.err != PortError::Ok ? kFlagError : 0);
  return s;
}
