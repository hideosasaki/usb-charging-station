// Native unit tests for MockPortReader. Verifies scenario A waypoints,
// detach/attach gating, override path, and that jitter stays within +/-2%.

#include <unity.h>

#include "../../src/mock_port_reader.h"

namespace {

constexpr uint32_t kSeed = 0xC0FFEEu;

void approx_eq(uint16_t expected, uint16_t actual, uint16_t tol, const char* what) {
  int diff = (int)actual - (int)expected;
  if (diff < 0) diff = -diff;
  char msg[96];
  snprintf(msg, sizeof(msg), "%s expected=%u actual=%u tol=%u",
           what, expected, actual, tol);
  TEST_ASSERT_TRUE_MESSAGE((uint16_t)diff <= tol, msg);
}

// Allows expected * pct% + 1 rounding slack. Use this for jitter-bound
// scenario readings rather than spelling out the tolerance literal.
void approx_pct(uint16_t expected, uint16_t actual, uint8_t pct, const char* what) {
  approx_eq(expected, actual, (uint16_t)(expected * pct / 100 + 1), what);
}

}  // namespace

void test_scenario_a_detached_before_2s(void) {
  MockPortReader r(0, ScenarioId::A_Pd30Phone, kSeed);
  r.begin();
  PortReading s = r.read(0);
  TEST_ASSERT_FALSE(s.attached());
  TEST_ASSERT_EQUAL_UINT16(0, s.v_mV);
  TEST_ASSERT_EQUAL_UINT16(0, s.i_c_mA);
}

void test_scenario_a_handshake_at_3s(void) {
  MockPortReader r(0, ScenarioId::A_Pd30Phone, kSeed);
  r.begin();
  PortReading s = r.read(3000);
  TEST_ASSERT_TRUE(s.attached());
  approx_eq(5000, s.v_mV, 50, "v@3s");
  approx_eq(300, s.i_c_mA, 10, "i@3s");
  TEST_ASSERT_EQUAL_UINT8((uint8_t)Protocol::Std5V, (uint8_t)s.proto);
}

void test_scenario_a_cc_at_10s(void) {
  MockPortReader r(0, ScenarioId::A_Pd30Phone, kSeed);
  r.begin();
  PortReading s = r.read(10'000);
  TEST_ASSERT_TRUE(s.attached());
  approx_eq(9000, s.v_mV, 50, "v@10s");
  approx_pct(2050, s.i_c_mA, 2, "i@10s");
  TEST_ASSERT_EQUAL_UINT8((uint8_t)Protocol::Pd30, (uint8_t)s.proto);
}

void test_scenario_a_cv_at_400s(void) {
  MockPortReader r(0, ScenarioId::A_Pd30Phone, kSeed);
  r.begin();
  PortReading s = r.read(400'000);
  TEST_ASSERT_TRUE(s.attached());
  approx_eq(9000, s.v_mV, 50, "v@400s");
  // CV taper is linear 2050 mA -> 300 mA across 305..605s, so t=400s lands
  // at ~1496 mA before +/-2% jitter.
  approx_eq(1496, s.i_c_mA, 35, "i@400s");
}

void test_scenario_a_neardone_at_700s(void) {
  MockPortReader r(0, ScenarioId::A_Pd30Phone, kSeed);
  r.begin();
  PortReading s = r.read(700'000);
  TEST_ASSERT_TRUE(s.attached());
  approx_eq(5000, s.v_mV, 50, "v@700s");
  approx_eq(300, s.i_c_mA, 15, "i@700s");
}

void test_scenario_a_detach_at_725s(void) {
  MockPortReader r(0, ScenarioId::A_Pd30Phone, kSeed);
  r.begin();
  PortReading s = r.read(725'000);
  TEST_ASSERT_FALSE(s.attached());
}

void test_scenario_a_loops_after_740s(void) {
  MockPortReader r(0, ScenarioId::A_Pd30Phone, kSeed);
  r.begin();
  PortReading s = r.read(750'000);
  TEST_ASSERT_TRUE(s.attached());
  approx_eq(9000, s.v_mV, 50, "v@loop");
  approx_pct(2050, s.i_c_mA, 2, "i@loop");
}

void test_scenario_c_idle_and_burst(void) {
  MockPortReader r(2, ScenarioId::C_IdleBurst, kSeed);
  r.begin();
  PortReading idle = r.read(10'000);
  TEST_ASSERT_FALSE(idle.attached());
  PortReading burst = r.read(150'000);
  TEST_ASSERT_TRUE(burst.attached());
  approx_eq(12000, burst.v_mV, 80, "vC burst");
  approx_pct(1500, burst.i_c_mA, 2, "iC burst");
  TEST_ASSERT_EQUAL_UINT8((uint8_t)Protocol::Qc30, (uint8_t)burst.proto);
}

void test_override_pins_value(void) {
  MockPortReader r(0, ScenarioId::A_Pd30Phone, kSeed);
  r.begin();
  r.set_override(9000, 2050, 0, Protocol::Pd30);
  PortReading s = r.read(0);  // would normally be detached
  TEST_ASSERT_TRUE(s.attached());
  TEST_ASSERT_EQUAL_UINT16(9000, s.v_mV);
  TEST_ASSERT_EQUAL_UINT16(2050, s.i_c_mA);
  r.clear_override();
  PortReading s2 = r.read(0);
  TEST_ASSERT_FALSE(s2.attached());
}

void test_force_detach_overrides_scenario(void) {
  MockPortReader r(0, ScenarioId::A_Pd30Phone, kSeed);
  r.begin();
  r.force_detach();
  PortReading s = r.read(10'000);
  TEST_ASSERT_FALSE(s.attached());
  r.force_attach();
  PortReading s2 = r.read(10'000);
  TEST_ASSERT_TRUE(s2.attached());
}

void test_force_attach_in_detached_window(void) {
  // Scenario A is detached 720..740s; force_attach must still report
  // attached using the 5V handshake fallback.
  MockPortReader r(0, ScenarioId::A_Pd30Phone, kSeed);
  r.begin();
  r.force_attach();
  PortReading s = r.read(730'000);
  TEST_ASSERT_TRUE(s.attached());
  approx_eq(5000, s.v_mV, 50, "v force_attach gap");
}

void test_jitter_within_two_percent(void) {
  MockPortReader r(0, ScenarioId::A_Pd30Phone, kSeed);
  r.begin();
  uint16_t lo = 0xFFFFu, hi = 0;
  for (uint32_t t = 10'000; t < 300'000; t += 1000) {
    PortReading s = r.read(t);
    if (s.i_c_mA < lo) lo = s.i_c_mA;
    if (s.i_c_mA > hi) hi = s.i_c_mA;
  }
  TEST_ASSERT_TRUE(lo >= (uint16_t)(2050 * 98 / 100 - 1));
  TEST_ASSERT_TRUE(hi <= (uint16_t)(2050 * 102 / 100 + 1));
}

void test_scenario_b_c_alone(void) {
  MockPortReader r(1, ScenarioId::B_DualCpA, kSeed);
  r.begin();
  PortReading s = r.read(100'000);  // mid first 300s window
  TEST_ASSERT_TRUE(s.has_c());
  TEST_ASSERT_FALSE(s.has_a());
  approx_eq(9000, s.v_mV, 50, "vB@100s");
  approx_pct(2050, s.i_c_mA, 2, "icB@100s");
  TEST_ASSERT_EQUAL_UINT16(0, s.i_a_mA);
}

void test_scenario_b_both_rails(void) {
  MockPortReader r(1, ScenarioId::B_DualCpA, kSeed);
  r.begin();
  PortReading s = r.read(450'000);  // mid 300..600s window
  TEST_ASSERT_TRUE(s.has_c());
  TEST_ASSERT_TRUE(s.has_a());
  approx_eq(5000, s.v_mV, 50, "vB@450s");
  approx_pct(1500, s.i_c_mA, 2, "icB@450s");
  approx_pct(800,  s.i_a_mA, 2, "iaB@450s");
}

void test_scenario_b_a_alone(void) {
  MockPortReader r(1, ScenarioId::B_DualCpA, kSeed);
  r.begin();
  PortReading s = r.read(750'000);  // mid 600..900s window
  TEST_ASSERT_FALSE(s.has_c());
  TEST_ASSERT_TRUE(s.has_a());
  approx_eq(5000, s.v_mV, 50, "vB@750s");
  TEST_ASSERT_EQUAL_UINT16(0, s.i_c_mA);
  approx_pct(1000, s.i_a_mA, 2, "iaB@750s");
}

void test_override_pins_dual_rail(void) {
  MockPortReader r(0, ScenarioId::A_Pd30Phone, kSeed);
  r.begin();
  r.set_override(5000, 1500, 800, Protocol::Std5V);
  PortReading s = r.read(0);
  TEST_ASSERT_TRUE(s.has_c());
  TEST_ASSERT_TRUE(s.has_a());
  TEST_ASSERT_EQUAL_UINT16(5000, s.v_mV);
  TEST_ASSERT_EQUAL_UINT16(1500, s.i_c_mA);
  TEST_ASSERT_EQUAL_UINT16(800,  s.i_a_mA);
}

void setUp(void) {}
void tearDown(void) {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_scenario_a_detached_before_2s);
  RUN_TEST(test_scenario_a_handshake_at_3s);
  RUN_TEST(test_scenario_a_cc_at_10s);
  RUN_TEST(test_scenario_a_cv_at_400s);
  RUN_TEST(test_scenario_a_neardone_at_700s);
  RUN_TEST(test_scenario_a_detach_at_725s);
  RUN_TEST(test_scenario_a_loops_after_740s);
  RUN_TEST(test_scenario_b_c_alone);
  RUN_TEST(test_scenario_b_both_rails);
  RUN_TEST(test_scenario_b_a_alone);
  RUN_TEST(test_scenario_c_idle_and_burst);
  RUN_TEST(test_override_pins_value);
  RUN_TEST(test_override_pins_dual_rail);
  RUN_TEST(test_force_detach_overrides_scenario);
  RUN_TEST(test_force_attach_in_detached_window);
  RUN_TEST(test_jitter_within_two_percent);
  return UNITY_END();
}
