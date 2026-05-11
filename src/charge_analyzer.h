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

struct EtaSeconds {
  uint32_t seconds;
  bool     valid;
};

// Recent window 5s, older window 30s -> the two averages are 25s apart.
static constexpr uint16_t kEtaWindowSpan_s    = 25;
static constexpr size_t   kEtaRecentWindow_s  = 5;
static constexpr size_t   kEtaOldWindow_s     = 30;

// Estimate remaining time from the recent slope of i_mA. Returns invalid
// when the current is steady (CC) or rising, when peak isn't established,
// or when not in a tapering phase. Uses the 30s-old vs 5s-recent average
// from PortHistory; the caller passes both already-computed values so
// this stays unit-test friendly without depending on PortHistory.
inline EtaSeconds eta_seconds(uint16_t now_i_mA, uint16_t avg_recent_mA,
                              uint16_t avg_old_mA, Phase phase) {
  if (phase == Phase::Idle || phase == Phase::Done) return {0, false};
  if (avg_old_mA <= avg_recent_mA) return {0, false};
  uint32_t drop_mA = avg_old_mA - avg_recent_mA;
  // Below the +/-2% mock-noise floor the slope is just jitter.
  if (drop_mA < 25) return {0, false};
  uint32_t secs = (uint32_t)now_i_mA * kEtaWindowSpan_s / drop_mA;
  // Beyond a few days the slope is noise-dominated; the UI also has no
  // column width for h-counts that large.
  if (secs > 99u * 3600u) return {0, false};
  return {secs, true};
}

