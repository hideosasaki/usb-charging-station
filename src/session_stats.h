// Per-port charging session: detects plug/unplug edges, tracks elapsed
// time, and accumulates energy. The Controller calls session_update once
// per sample with the measured dt_ms.

#pragma once

#include <stdint.h>

#include "port_reader.h"

struct SessionStats {
  uint32_t start_ms;
  uint32_t energy_mWh;
  bool     active;
  bool     last_attached;
};

void session_reset(SessionStats& s);
void session_update(SessionStats& s, const PortReading& r, uint32_t dt_ms);
