// Compile-time switches that select between mock and real hardware
// implementations. Defaults are tuned so a bare `pio run` builds the mock
// firmware suitable for display-side development without SW3518 hardware.

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifndef USE_MOCK_PORTS
#define USE_MOCK_PORTS 1
#endif

// Sampling tick — V/A/W readings update at this cadence. Picking 100 ms
// trades 1 hour of history retention (1 Hz × 3600 samples) for ~6 min,
// which still exceeds the longest analyzer window (60 s).
inline constexpr uint32_t kSampleMs      = 100;
inline constexpr size_t   kSamplesPerSec = 1000 / kSampleMs;

// Sample-count equivalent of a real-time window. Keep callers in the
// "seconds" world; this is the only place that knows the sampling rate.
inline constexpr size_t samples_for(size_t seconds) {
  return seconds * kSamplesPerSec;
}

// How many SW3518 ports are physically wired. Used only when
// USE_MOCK_PORTS=0; make_port_reader() returns nullptr for indices
// at or above this count.
#ifndef SW3518_PORT_COUNT
#define SW3518_PORT_COUNT 1
#endif
