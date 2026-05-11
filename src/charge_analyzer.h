// Classifies a port's current charging phase from a recent history
// window. Stateless: each call recomputes from PortHistory + the latest
// live reading, so View code can call it freely.

#pragma once

#include <stdint.h>

#include "port_history.h"
#include "port_reader.h"

enum class Phase : uint8_t { Idle, CC, CV, NearDone, Done };

Phase analyze(const PortHistory& h, const PortReading& now);

inline const char* phase_name(Phase p) {
  switch (p) {
    case Phase::Idle:     return "idle";
    case Phase::CC:       return "CC";
    case Phase::CV:       return "CV";
    case Phase::NearDone: return "near";
    case Phase::Done:     return "done";
  }
  return "?";
}
