// Native unit tests for SessionStats. Verifies plug/unplug edges,
// elapsed-time reset, and energy accumulation in mWh.

#include <unity.h>

#include "../../src/port_reader.h"
#include "../../src/session_stats.h"

namespace {

PortReading mk(uint16_t v_mV, uint16_t i_mA, bool attached, uint32_t t_ms) {
  PortReading r{};
  r.t_ms      = t_ms;
  r.v_mV      = v_mV;
  r.i_c_mA    = i_mA;
  r.i_a_mA    = 0;
  r.attached  = attached;
  r.rail_mask = attached ? kRailMaskC : 0;
  return r;
}

}  // namespace

void test_initial_state_inactive(void) {
  SessionStats s{};
  TEST_ASSERT_FALSE(s.active);
  TEST_ASSERT_EQUAL_UINT32(0, s.energy_mWh);
}

void test_attach_starts_session(void) {
  SessionStats s{};
  session_update(s, mk(5000, 300, true, 1000), 1000);
  TEST_ASSERT_TRUE(s.active);
  TEST_ASSERT_EQUAL_UINT32(1000, s.start_ms);
}

void test_detach_stops_session_but_keeps_energy(void) {
  SessionStats s{};
  session_update(s, mk(9000, 2000, true, 1000), 1000);  // start
  session_update(s, mk(9000, 2000, true, 2000), 1000);  // accumulate
  uint32_t e_before = s.energy_mWh;
  TEST_ASSERT_TRUE(e_before > 0);
  session_update(s, mk(0, 0, false, 3000), 1000);
  TEST_ASSERT_FALSE(s.active);
  TEST_ASSERT_EQUAL_UINT32(e_before, s.energy_mWh);
}

void test_reattach_resets_energy_and_start(void) {
  SessionStats s{};
  session_update(s, mk(9000, 2000, true, 1000), 1000);
  session_update(s, mk(9000, 2000, true, 2000), 1000);
  session_update(s, mk(0, 0, false, 3000), 1000);
  session_update(s, mk(9000, 2000, true, 10000), 1000);
  TEST_ASSERT_TRUE(s.active);
  TEST_ASSERT_EQUAL_UINT32(10000, s.start_ms);
  TEST_ASSERT_EQUAL_UINT32(0, s.energy_mWh);
}

void test_energy_scales_with_dt(void) {
  // 9V * 2A * 1h = 18 Wh = 18000 mWh
  SessionStats s{};
  session_update(s, mk(9000, 2000, true, 0), 1000);  // attach edge, no dt
  // Drive an hour of 1s ticks.
  for (uint32_t t = 1; t <= 3600; ++t) {
    session_update(s, mk(9000, 2000, true, t * 1000), 1000);
  }
  // Allow +/-1 mWh rounding.
  TEST_ASSERT_TRUE(s.energy_mWh >= 17999 && s.energy_mWh <= 18001);
}

void test_reset_clears_all(void) {
  SessionStats s{};
  session_update(s, mk(9000, 2000, true, 1000), 1000);
  session_update(s, mk(9000, 2000, true, 2000), 1000);
  session_reset(s);
  TEST_ASSERT_FALSE(s.active);
  TEST_ASSERT_EQUAL_UINT32(0, s.energy_mWh);
  TEST_ASSERT_EQUAL_UINT32(0, s.start_ms);
  TEST_ASSERT_EQUAL_UINT16(0, s.peak_i_mA);
}

void test_peak_current_is_session_max(void) {
  SessionStats s{};
  session_update(s, mk(5000, 300, true, 0), 1000);
  session_update(s, mk(9000, 2000, true, 1000), 1000);
  session_update(s, mk(9000, 1500, true, 2000), 1000);
  TEST_ASSERT_EQUAL_UINT16(2000, s.peak_i_mA);
}

void test_peak_resets_on_reattach(void) {
  SessionStats s{};
  session_update(s, mk(9000, 2000, true, 0), 1000);
  session_update(s, mk(0, 0, false, 1000), 1000);
  session_update(s, mk(5000, 300, true, 2000), 1000);
  TEST_ASSERT_EQUAL_UINT16(300, s.peak_i_mA);
}

void setUp(void) {}
void tearDown(void) {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_initial_state_inactive);
  RUN_TEST(test_attach_starts_session);
  RUN_TEST(test_detach_stops_session_but_keeps_energy);
  RUN_TEST(test_reattach_resets_energy_and_start);
  RUN_TEST(test_energy_scales_with_dt);
  RUN_TEST(test_reset_clears_all);
  RUN_TEST(test_peak_current_is_session_max);
  RUN_TEST(test_peak_resets_on_reattach);
  return UNITY_END();
}
