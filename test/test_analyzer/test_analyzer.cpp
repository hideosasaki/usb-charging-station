// Native unit tests for ChargeAnalyzer phase classification.

#include <unity.h>

#include "../../src/build_config.h"
#include "../../src/charge_analyzer.h"
#include "../../src/port_bridge.h"
#include "../../src/port_history.h"
#include "../../src/port_reader.h"
#include "../support/port_fixtures.h"

namespace {

PortReading mk(uint16_t v_mV, uint16_t i_mA, bool attached = true) {
  return test_support::make_reading_c(v_mV, i_mA, attached);
}

// `seconds` is in real time; convert to the underlying sample count so
// the test wall-clock semantics survive a change in kSampleMs.
void fill_seconds(PortHistory& h, const PortReading& r, size_t seconds) {
  for (size_t i = 0; i < samples_for(seconds); ++i) h.push(to_sample(r));
}

}  // namespace

void test_idle_when_detached(void) {
  PortHistory h;
  PortReading now = mk(0, 0, false);
  TEST_ASSERT_EQUAL_UINT8((uint8_t)Phase::Idle, (uint8_t)analyze(h, Rail::UsbC, now));
}

void test_idle_when_current_negligible(void) {
  PortHistory h;
  PortReading now = mk(5000, 20, true);
  fill_seconds(h, now, 10);
  TEST_ASSERT_EQUAL_UINT8((uint8_t)Phase::Idle, (uint8_t)analyze(h, Rail::UsbC, now));
}

void test_cc_high_v_high_stable_i(void) {
  PortHistory h;
  PortReading now = mk(9000, 2050);
  fill_seconds(h, now, 60);
  TEST_ASSERT_EQUAL_UINT8((uint8_t)Phase::CC, (uint8_t)analyze(h, Rail::UsbC, now));
}

void test_cv_high_v_falling_i(void) {
  PortHistory h;
  // 60s ago: ~2000 mA, now: 1000 mA at 9V — current dropped meaningfully.
  for (size_t i = 0; i < samples_for(30); ++i) h.push(to_sample(mk(9000, 2000)));
  for (size_t i = 0; i < samples_for(30); ++i) h.push(to_sample(mk(9000, 1000)));
  PortReading now = mk(9000, 1000);
  TEST_ASSERT_EQUAL_UINT8((uint8_t)Phase::CV, (uint8_t)analyze(h, Rail::UsbC, now));
}

void test_neardone_low_v_short_history(void) {
  // Just transitioned to 5V/450mA — not enough Done-window samples yet.
  PortHistory h;
  PortReading now = mk(5000, 450);
  fill_seconds(h, now, 5);
  TEST_ASSERT_EQUAL_UINT8((uint8_t)Phase::NearDone, (uint8_t)analyze(h, Rail::UsbC, now));
}

void test_done_after_neardone_persists(void) {
  PortHistory h;
  PortReading now = mk(5000, 250);
  fill_seconds(h, now, 60);
  TEST_ASSERT_EQUAL_UINT8((uint8_t)Phase::Done, (uint8_t)analyze(h, Rail::UsbC, now));
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

void test_eta_invalid_in_cc(void) {
  // CC: averages are flat at ~2000 mA -> no taper, no ETA.
  EtaSeconds e = eta_seconds(2000, 2000, 2000, Phase::CC);
  TEST_ASSERT_FALSE(e.valid);
}

void test_eta_invalid_when_idle(void) {
  EtaSeconds e = eta_seconds(0, 0, 0, Phase::Idle);
  TEST_ASSERT_FALSE(e.valid);
}

void test_eta_invalid_when_done(void) {
  EtaSeconds e = eta_seconds(200, 200, 250, Phase::Done);
  TEST_ASSERT_FALSE(e.valid);
}

void test_eta_in_cv(void) {
  // 25s ago: 1500 mA; recently: 1000 mA; drop = 500 mA over 25s = 20 mA/s.
  // now_i = 1000 -> remaining ~50s.
  EtaSeconds e = eta_seconds(1000, 1000, 1500, Phase::CV);
  TEST_ASSERT_TRUE(e.valid);
  TEST_ASSERT_TRUE(e.seconds >= 45 && e.seconds <= 55);
}

void test_eta_invalid_when_rising(void) {
  EtaSeconds e = eta_seconds(1000, 1100, 900, Phase::CV);
  TEST_ASSERT_FALSE(e.valid);
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
  RUN_TEST(test_eta_invalid_in_cc);
  RUN_TEST(test_eta_invalid_when_idle);
  RUN_TEST(test_eta_invalid_when_done);
  RUN_TEST(test_eta_in_cv);
  RUN_TEST(test_eta_invalid_when_rising);
  return UNITY_END();
}
