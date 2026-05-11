// Energy accumulator and attach-edge detector. dt_ms is supplied by the
// Controller so the same code runs unchanged in native tests.

#include "session_stats.h"

void session_reset(SessionStats& s) {
  s.start_ms      = 0;
  s.energy_mWh    = 0;
  s.peak_i_mA     = 0;
  s.active        = false;
  s.last_attached = false;
}

void session_update(SessionStats& s, const PortReading& r, Rail rail,
                    uint32_t dt_ms) {
  uint16_t i_mA     = r.i_mA(rail);
  bool     attached = r.has(rail);

  bool rising = attached && !s.last_attached;
  if (rising) {
    s.start_ms   = r.t_ms;
    s.energy_mWh = 0;
    s.peak_i_mA  = 0;
    s.active     = true;
  } else if (!attached) {
    s.active = false;
  }
  s.last_attached = attached;

  if (s.active) {
    if (i_mA > s.peak_i_mA) s.peak_i_mA = i_mA;
    if (!rising && i_mA > 0) {
      // mV * mA / 1000 = uW; * dt_ms / 3_600_000 = mWh.
      // Combine into a single 64-bit multiply to keep precision on M0+.
      uint64_t uw  = (uint64_t)r.v_mV * (uint64_t)i_mA / 1000ull;
      uint64_t mwh = uw * (uint64_t)dt_ms / 3'600'000ull;
      s.energy_mWh += (uint32_t)mwh;
    }
  }
}
