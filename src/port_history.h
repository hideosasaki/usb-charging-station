// Per-port time-series ring buffer. One sample per second, ~1 hour of
// retention. Samples are stored newest-first via at(0) regardless of the
// internal write cursor; callers never see the wrap.

#pragma once

#include <stddef.h>
#include <stdint.h>

// flags bits — keep one definition so producers (PortReader bridge) and
// consumers (View) cannot drift apart on the bit layout.
static constexpr uint8_t kFlagAttached = 0x01;
static constexpr uint8_t kFlagError    = 0x02;

inline uint32_t power_mW(uint16_t v_mV, uint16_t i_mA) {
  return (static_cast<uint32_t>(v_mV) * static_cast<uint32_t>(i_mA)) / 1000u;
}

struct HistorySample {
  uint16_t v_mV;
  uint16_t i_mA;
  uint8_t  proto;
  uint8_t  flags;
  uint16_t reserved;
};
static_assert(sizeof(HistorySample) == 8,
              "HistorySample must remain 8 bytes for the SRAM budget");

class PortHistory {
 public:
  static constexpr size_t kCapacity = 3600;

  PortHistory();

  void   push(const HistorySample& s);
  size_t size() const { return size_; }

  // Newest-first random access. at(0) is the most recently pushed sample.
  // Behavior is undefined when i >= size().
  const HistorySample& at(size_t i) const;

  // Mean current over the last min(seconds, size()) samples. Returns 0 when
  // empty.
  uint16_t avg_i_mA(size_t seconds) const;

  // Min/max instantaneous power (mW) over the last min(seconds, size())
  // samples. Returns lo=hi=0 when empty.
  void power_range_mW(size_t seconds, uint32_t& lo, uint32_t& hi) const;

 private:
  HistorySample buf_[kCapacity];
  size_t        head_;   // index where the next push will write
  size_t        size_;   // number of valid entries (<= kCapacity)
};
