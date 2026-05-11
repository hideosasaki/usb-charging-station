// Native tests for the Serial command parser. Exercises dispatch
// without touching Arduino Serial.

#include <string.h>

#include <unity.h>

#include "../../src/mock_port_reader.h"
#include "../../src/serial_cmd.h"

namespace {

constexpr uint32_t kSeed = 0xC0FFEEu;

MockPortReader r0(0, ScenarioId::A_Pd30Phone,   kSeed);
MockPortReader r1(1, ScenarioId::B_Std5VSteady, kSeed);
MockPortReader r2(2, ScenarioId::C_IdleBurst,   kSeed);
MockPortReader* readers[3] = {&r0, &r1, &r2};

CmdResult run(const char* line) {
  char buf[64];
  strncpy(buf, line, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';
  char out[64] = {0};
  return serial_cmd_dispatch(buf, readers, out, sizeof(out));
}

}  // namespace

void setUp(void) {
  // Reset each port to auto/scenario default before every test.
  r0.set_scenario(ScenarioId::A_Pd30Phone);
  r1.set_scenario(ScenarioId::B_Std5VSteady);
  r2.set_scenario(ScenarioId::C_IdleBurst);
  r0.clear_override(); r1.clear_override(); r2.clear_override();
}
void tearDown(void) {}

void test_unknown_command(void) {
  TEST_ASSERT_EQUAL_INT((int)CmdResult::Unknown, (int)run("hello"));
}

void test_port_detach(void) {
  TEST_ASSERT_EQUAL_INT((int)CmdResult::Ok, (int)run("port 0 detach"));
  PortReading r = r0.read(10'000);  // would normally be attached at t=10s
  TEST_ASSERT_FALSE(r.attached);
}

void test_port_attach(void) {
  TEST_ASSERT_EQUAL_INT((int)CmdResult::Ok, (int)run("port 0 attach"));
  PortReading r = r0.read(0);  // would normally be detached at t=0
  TEST_ASSERT_TRUE(r.attached);
}

void test_port_auto_restores_scenario(void) {
  run("port 0 detach");
  TEST_ASSERT_EQUAL_INT((int)CmdResult::Ok, (int)run("port 0 auto"));
  PortReading r = r0.read(10'000);
  TEST_ASSERT_TRUE(r.attached);
}

void test_port_override_fixed_value(void) {
  TEST_ASSERT_EQUAL_INT((int)CmdResult::Ok,
                       (int)run("port 0 9000 2050 PD30"));
  PortReading r = r0.read(0);  // even when scenario says detached
  TEST_ASSERT_TRUE(r.attached);
  TEST_ASSERT_EQUAL_UINT16(9000, r.v_mV);
  TEST_ASSERT_EQUAL_UINT16(2050, r.i_c_mA);
  TEST_ASSERT_EQUAL_UINT8((uint8_t)Protocol::Pd30, (uint8_t)r.proto);
}

void test_port_override_protocol_aliases(void) {
  TEST_ASSERT_EQUAL_INT((int)CmdResult::Ok,
                       (int)run("port 1 5000 480 STD5V"));
  PortReading r = r1.read(0);
  TEST_ASSERT_EQUAL_UINT8((uint8_t)Protocol::Std5V, (uint8_t)r.proto);
}

void test_port_index_out_of_range(void) {
  TEST_ASSERT_EQUAL_INT((int)CmdResult::OutOfRange,
                       (int)run("port 9 detach"));
}

void test_port_bad_args(void) {
  TEST_ASSERT_EQUAL_INT((int)CmdResult::BadArgs, (int)run("port"));
  TEST_ASSERT_EQUAL_INT((int)CmdResult::BadArgs, (int)run("port 0"));
  TEST_ASSERT_EQUAL_INT((int)CmdResult::BadArgs,
                       (int)run("port 0 9000 2050"));  // missing proto
}

void test_scenario_switch(void) {
  TEST_ASSERT_EQUAL_INT((int)CmdResult::Ok, (int)run("scenario 0 B"));
  PortReading r = r0.read(0);
  TEST_ASSERT_TRUE(r.attached);
  TEST_ASSERT_EQUAL_UINT16(5000, r.v_mV);
}

void test_scenario_bad_id(void) {
  TEST_ASSERT_EQUAL_INT((int)CmdResult::BadArgs,
                       (int)run("scenario 0 Z"));
}

void test_status_writes_output(void) {
  char buf[128] = {0};
  char line[16] = "status";
  CmdResult res = serial_cmd_dispatch(line, readers, buf, sizeof(buf));
  TEST_ASSERT_EQUAL_INT((int)CmdResult::Ok, (int)res);
  TEST_ASSERT_TRUE(strlen(buf) > 0);
}

void test_empty_line_is_ok(void) {
  TEST_ASSERT_EQUAL_INT((int)CmdResult::Ok, (int)run(""));
  TEST_ASSERT_EQUAL_INT((int)CmdResult::Ok, (int)run("   "));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_unknown_command);
  RUN_TEST(test_port_detach);
  RUN_TEST(test_port_attach);
  RUN_TEST(test_port_auto_restores_scenario);
  RUN_TEST(test_port_override_fixed_value);
  RUN_TEST(test_port_override_protocol_aliases);
  RUN_TEST(test_port_index_out_of_range);
  RUN_TEST(test_port_bad_args);
  RUN_TEST(test_scenario_switch);
  RUN_TEST(test_scenario_bad_id);
  RUN_TEST(test_status_writes_output);
  RUN_TEST(test_empty_line_is_ok);
  return UNITY_END();
}
