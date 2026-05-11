#include "port_history.h"

PortHistory::PortHistory() : head_(0), size_(0) {}

void PortHistory::push(const HistorySample& s) {
  buf_[head_] = s;
  head_ = (head_ + 1) % kCapacity;
  if (size_ < kCapacity) ++size_;
}

const HistorySample& PortHistory::at(size_t i) const {
  size_t idx = (head_ + kCapacity - 1 - i) % kCapacity;
  return buf_[idx];
}

namespace {

// Start at the newest sample and walk backwards. kCapacity is not a power
// of two, so a single modulo per step would compile to a software divide
// on Cortex-M0+; the branch-wrap below avoids ~n divides per query.
struct ReverseCursor {
  const HistorySample* buf;
  size_t               idx;
  void advance(size_t cap) { idx = (idx == 0) ? (cap - 1) : (idx - 1); }
};

ReverseCursor newest_cursor(const HistorySample* buf, size_t head,
                            size_t cap) {
  return ReverseCursor{buf, (head == 0) ? (cap - 1) : (head - 1)};
}

}  // namespace

uint16_t PortHistory::avg_i_mA(size_t seconds) const {
  if (size_ == 0) return 0;
  size_t   n   = seconds < size_ ? seconds : size_;
  uint32_t sum = 0;
  auto     cur = newest_cursor(buf_, head_, kCapacity);
  for (size_t i = 0; i < n; ++i) {
    sum += buf_[cur.idx].i_mA;
    cur.advance(kCapacity);
  }
  return static_cast<uint16_t>(sum / n);
}

void PortHistory::power_range_mW(size_t seconds, uint32_t& lo,
                                 uint32_t& hi) const {
  if (size_ == 0) {
    lo = 0;
    hi = 0;
    return;
  }
  size_t   n   = seconds < size_ ? seconds : size_;
  // Track min/max of v*i in mV*mA (uint32) and divide once at the end —
  // saves n software /1000 divides on M0+.
  uint32_t mn  = 0xFFFFFFFFu;
  uint32_t mx  = 0;
  auto     cur = newest_cursor(buf_, head_, kCapacity);
  for (size_t i = 0; i < n; ++i) {
    const auto& s  = buf_[cur.idx];
    uint32_t    vi = static_cast<uint32_t>(s.v_mV) *
                     static_cast<uint32_t>(s.i_mA);
    if (vi < mn) mn = vi;
    if (vi > mx) mx = vi;
    cur.advance(kCapacity);
  }
  // Defer the /1000 (single divide on M0+) until after the min/max loop.
  lo = mn / 1000u;
  hi = mx / 1000u;
}
