// Ring-buffer implementation for PortHistory. Newest-first indexing is
// computed at read time so the producer side stays a single increment.

#include "port_history.h"

PortHistory::PortHistory() : head_(0), size_(0) {
  for (auto& s : buf_) s = HistorySample{};
}

void PortHistory::push(const HistorySample& s) {
  buf_[head_] = s;
  head_ = (head_ + 1) % kCapacity;
  if (size_ < kCapacity) ++size_;
}

const HistorySample& PortHistory::at(size_t i) const {
  // Newest sample sits at (head_ - 1) modulo kCapacity. at(0) -> newest,
  // at(size_-1) -> oldest still in the buffer.
  size_t idx = (head_ + kCapacity - 1 - i) % kCapacity;
  return buf_[idx];
}

uint16_t PortHistory::avg_i_mA(size_t seconds) const {
  if (size_ == 0) return 0;
  size_t n = seconds < size_ ? seconds : size_;
  uint32_t sum = 0;
  for (size_t i = 0; i < n; ++i) sum += at(i).i_mA;
  return static_cast<uint16_t>(sum / n);
}

void PortHistory::power_range_mW(size_t seconds, uint32_t& lo,
                                 uint32_t& hi) const {
  if (size_ == 0) {
    lo = 0;
    hi = 0;
    return;
  }
  size_t n = seconds < size_ ? seconds : size_;
  uint32_t mn = 0xFFFFFFFFu;
  uint32_t mx = 0;
  for (size_t i = 0; i < n; ++i) {
    const auto& s = at(i);
    // mV * mA / 1000 = mW. Cast to 32-bit before multiplying.
    uint32_t mw = (static_cast<uint32_t>(s.v_mV) *
                   static_cast<uint32_t>(s.i_mA)) / 1000u;
    if (mw < mn) mn = mw;
    if (mw > mx) mx = mw;
  }
  lo = mn;
  hi = mx;
}
