// Phase classifier. Thresholds are intentionally loose — the goal is a
// human-readable badge on the UI, not a battery-management decision.

#include "charge_analyzer.h"

namespace {

constexpr uint16_t kHighVoltage_mV   = 7000;
constexpr uint16_t kIdleCurrent_mA   = 50;
constexpr uint16_t kNearDoneI_mA     = 500;
constexpr uint16_t kDoneI_mA         = 300;
constexpr uint16_t kCvDropThresh_mA  = 300;
constexpr size_t   kCvWindowOld_s    = 60;
constexpr size_t   kCvWindowRecent_s = 10;
constexpr size_t   kDoneWindow_s     = 30;

}  // namespace

Phase analyze(const PortHistory& h, const PortReading& now) {
  uint16_t now_i_mA = reading_total_i_mA(now);
  if (!now.attached || now_i_mA < kIdleCurrent_mA) return Phase::Idle;

  if (now.v_mV >= kHighVoltage_mV) {
    // CV is detected as a sustained drop from the older window to the
    // recent one. Without enough history we cannot distinguish CC from CV,
    // so default to CC.
    if (h.size() >= kCvWindowOld_s) {
      uint16_t old_avg    = h.avg_total_i_mA(kCvWindowOld_s);
      uint16_t recent_avg = h.avg_total_i_mA(kCvWindowRecent_s);
      if (old_avg > recent_avg + kCvDropThresh_mA) return Phase::CV;
    }
    return Phase::CC;
  }

  // Low-voltage tail. If we've sat at low-V/low-I for long enough call
  // it Done; otherwise it's still tapering toward done.
  if (now_i_mA <= kDoneI_mA && h.size() >= kDoneWindow_s &&
      h.avg_total_i_mA(kDoneWindow_s) <= kDoneI_mA) {
    return Phase::Done;
  }
  return Phase::NearDone;
}
