// Native unit tests for ChargeAnalyzer phase classification.

#include <unity.h>

#include "../../src/charge_analyzer.h"
#include "../../src/port_history.h"
#include "../../src/port_reader.h"

namespace {

PortReading mk(uint16_t v_mV, uint16_t i_mA, bool attached = true) {
  PortReading r{};
  r.v_mV = v_mV;
  r.i_mA = i_mA;
  r.proto = Protocol::Pd30;
  r.err = PortError::Ok;
  r.attached = attached;
  return r;
}

HistorySample to_sample(const PortReading& r) {
  HistorySample s{};
  s.v_mV = r.v_mV;
  s.i_mA = r.i_mA;
  s.proto = (uint8_t)r.proto;
  s.flags = r.attached ? kFlagAttached : 0;
  return s;
}

void fill(PortHistory& h, const PortReading& r, size_t n) {
  for (size_t i = 0; i < n; ++i) h.push(to_sample(r));
}

}  // namespace

void test_idle_when_detached(void) {
  PortHistory h;
  PortReading now = mk(0, 0, false);
  TEST_ASSERT_EQUAL_UINT8((uint8_t)Phase::Idle, (uint8_t)analyze(h, now));
}

void test_idle_when_current_negligible(void) {
  PortHistory h;
  PortReading now = mk(5000, 20, true);
  fill(h, now, 10);
  TEST_ASSERT_EQUAL_UINT8((uint8_t)Phase::Idle, (uint8_t)analyze(h, now));
}

void test_cc_high_v_high_stable_i(void) {
  PortHistory h;
  PortReading now = mk(9000, 2050);
  fill(h, now, 60);
  TEST_ASSERT_EQUAL_UINT8((uint8_t)Phase::CC, (uint8_t)analyze(h, now));
}

void test_cv_high_v_falling_i(void) {
  PortHistory h;
  // 60s ago: ~2000 mA, now: 1000 mA at 9V — current dropped meaningfully.
  for (int i = 0; i < 30; ++i) h.push(to_sample(mk(9000, 2000)));
  for (int i = 0; i < 30; ++i) h.push(to_sample(mk(9000, 1000)));
  PortReading now = mk(9000, 1000);
  TEST_ASSERT_EQUAL_UINT8((uint8_t)Phase::CV, (uint8_t)analyze(h, now));
}

void test_neardone_low_v_short_history(void) {
  // Just transitioned to 5V/450mA — not enough Done-window samples yet.
  PortHistory h;
  PortReading now = mk(5000, 450);
  fill(h, now, 5);
  TEST_ASSERT_EQUAL_UINT8((uint8_t)Phase::NearDone, (uint8_t)analyze(h, now));
}

void test_done_after_neardone_persists(void) {
  PortHistory h;
  PortReading now = mk(5000, 250);
  fill(h, now, 60);
  TEST_ASSERT_EQUAL_UINT8((uint8_t)Phase::Done, (uint8_t)analyze(h, now));
}

void test_progress_invalid_when_peak_below_threshold(void) {
  ChargeProgress p = charge_progress(300, 300, Phase::CC);
  TEST_ASSERT_FALSE(p.valid);
}

void test_progress_zero_at_cc_peak(void) {
  ChargeProgress p = charge_progress(2000, 2000, Phase::CC);
  TEST_ASSERT_TRUE(p.valid);
  TEST_ASSERT_EQUAL_UINT8(0, p.pct);
}

void test_progress_half_in_cv(void) {
  ChargeProgress p = charge_progress(2000, 1000, Phase::CV);
  TEST_ASSERT_TRUE(p.valid);
  TEST_ASSERT_EQUAL_UINT8(50, p.pct);
}

void test_progress_high_near_done(void) {
  ChargeProgress p = charge_progress(2000, 200, Phase::NearDone);
  TEST_ASSERT_TRUE(p.valid);
  TEST_ASSERT_EQUAL_UINT8(90, p.pct);
}

void test_progress_clamps_at_100_when_done(void) {
  ChargeProgress p = charge_progress(0, 0, Phase::Done);
  TEST_ASSERT_TRUE(p.valid);
  TEST_ASSERT_EQUAL_UINT8(100, p.pct);
}

void setUp(void) {}
void tearDown(void) {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_idle_when_detached);
  RUN_TEST(test_idle_when_current_negligible);
  RUN_TEST(test_cc_high_v_high_stable_i);
  RUN_TEST(test_cv_high_v_falling_i);
  RUN_TEST(test_neardone_low_v_short_history);
  RUN_TEST(test_done_after_neardone_persists);
  RUN_TEST(test_progress_invalid_when_peak_below_threshold);
  RUN_TEST(test_progress_zero_at_cc_peak);
  RUN_TEST(test_progress_half_in_cv);
  RUN_TEST(test_progress_high_near_done);
  RUN_TEST(test_progress_clamps_at_100_when_done);
  return UNITY_END();
}
