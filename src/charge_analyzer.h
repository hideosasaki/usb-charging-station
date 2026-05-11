// Classifies a port's current charging phase from a recent history
// window. Stateless: each call recomputes from PortHistory + the latest
// live reading, so View code can call it freely.

#pragma once

#include <stdint.h>

#include "port_history.h"
#include "port_reader.h"

enum class Phase : uint8_t { Idle, CC, CV, NearDone, Done };

Phase analyze(const PortHistory& h, const PortReading& now);

// peak_i_mA below this threshold counts as "not yet established" — the
// handshake at 5V/0.3A would otherwise pin peak to a value that makes
// progress jump to 0% as soon as CC starts.
static constexpr uint16_t kPeakValidThreshold_mA = 500;

struct ChargeProgress {
  uint8_t pct;        // 0..100
  bool    valid;      // false when peak isn't yet established
};

inline ChargeProgress charge_progress(uint16_t peak_i_mA, uint16_t now_i_mA,
                                      Phase phase) {
  if (phase == Phase::Done) return {100, true};
  if (peak_i_mA < kPeakValidThreshold_mA) return {0, false};
  if (now_i_mA >= peak_i_mA) return {0, true};
  uint32_t num = (uint32_t)(peak_i_mA - now_i_mA) * 100u;
  uint32_t pct = num / peak_i_mA;
  if (pct > 100) pct = 100;
  return {(uint8_t)pct, true};
}

inline const char* phase_name(Phase p) {
  switch (p) {
    case Phase::Idle:     return "Idle";
    case Phase::CC:       return "Fast";
    case Phase::CV:       return "Taper";
    case Phase::NearDone: return "Almost";
    case Phase::Done:     return "Done";
  }
  return "?";
}
