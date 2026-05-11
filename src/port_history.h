// Per-port time-series ring buffer. push() rate is set by kSampleMs in
// build_config.h; at the default 10 Hz the 3600-slot buffer holds ~6
// minutes — long enough for every analyzer/ETA window. Samples are
// stored newest-first via at(0) regardless of the internal write cursor;
// callers never see the wrap.

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "build_config.h"
#include "port_reader.h"  // Rail

// flags bits — keep one definition so producers (PortReader bridge) and
// consumers (View) cannot drift apart on the bit layout.
static constexpr uint8_t kFlagAttached = 0x01;
static constexpr uint8_t kFlagError    = 0x02;

inline uint32_t power_mW(uint16_t v_mV, uint16_t i_mA) {
  return (static_cast<uint32_t>(v_mV) * static_cast<uint32_t>(i_mA)) / 1000u;
}

// 12 bytes: Vbus shared, two rail currents preserved verbatim so the
// USB-C and USB-A sides can be analyzed independently. RAM budget:
// 3 ports * 3600 samples * 12 B = 130 KB, well within the 264 KB SRAM.
// The reserved word leaves headroom for future per-sample telemetry
// (e.g. chip temperature) without bumping the budget again.
struct HistorySample {
  uint16_t v_mV;
  uint16_t i_c_mA;     // USB-C rail current
  uint16_t i_a_mA;     // USB-A rail current
  uint8_t  proto;
  uint8_t  flags;      // see kFlag*; rail-mask bits live in bits 2..3
  uint32_t reserved;

  uint16_t i_mA(Rail rail) const {
    return rail == Rail::UsbC ? i_c_mA : i_a_mA;
  }
  uint32_t total_i_mA() const {
    return static_cast<uint32_t>(i_c_mA) + static_cast<uint32_t>(i_a_mA);
  }
};
static_assert(sizeof(HistorySample) == 12,
              "HistorySample size assumption changed; revisit SRAM budget");

class PortHistory {
 public:
  static constexpr size_t kCapacity = 3600;

  PortHistory();

  void   push(const HistorySample& s);
  size_t size() const { return size_; }

  // Newest-first random access. at(0) is the most recently pushed sample.
  // Behavior is undefined when i >= size().
  const HistorySample& at(size_t i) const;

  // Mean current of the given rail over the last n samples. Returns 0
  // when empty. Used by tests that reason directly in sample counts.
  uint16_t avg_i_mA_samples(size_t n, Rail rail) const;

  // Mean current over the last `seconds` of history. Returns 0 when empty.
  // Production callers express analyzer/ETA windows in seconds, so this
  // converts to the underlying sample count via kSamplesPerSec.
  uint16_t avg_i_mA(size_t seconds, Rail rail) const {
    return avg_i_mA_samples(samples_for(seconds), rail);
  }

  // Min/max instantaneous power (mW) over the last min(seconds, size())
  // samples. Power is computed against the per-sample rail sum.
  void power_range_mW(size_t seconds, uint32_t& lo, uint32_t& hi) const;

  // Compress the last `total_seconds` of power into `count` bins of
  // averaged mW, oldest bin at out[0]. Bins past the available history
  // are zero-filled. Used to drive a fixed-width sparkline.
  void power_downsample_mW(uint32_t* out, size_t count,
                           size_t total_seconds) const;

 private:
  HistorySample buf_[kCapacity];
  size_t        head_;   // index where the next push will write
  size_t        size_;   // number of valid entries (<= kCapacity)
};
