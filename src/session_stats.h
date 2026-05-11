// Per-port charging session: detects plug/unplug edges, tracks elapsed
// time, and accumulates energy. The Controller calls session_update once
// per sample with the measured dt_ms.

#pragma once

#include <stdint.h>

#include "port_reader.h"

struct SessionStats {
  uint32_t start_ms;
  uint32_t energy_mWh;
  uint16_t peak_i_mA;
  bool     active;
  bool     last_attached;
};

void session_reset(SessionStats& s);

// Update a single-rail session. Plug/unplug edges fire from the per-rail
// attach state, so a USB-A side session keeps running across a USB-C
// device swap and vice versa.
void session_update(SessionStats& s, const PortReading& r, Rail rail,
                    uint32_t dt_ms);

inline uint32_t session_elapsed_s(const SessionStats& s, uint32_t now_ms) {
  if (!s.active) return 0;
  return (now_ms - s.start_ms) / 1000u;
}
