// Native unit tests for PortHistory ring buffer. Verifies push/at order,
// size growth, capacity wrap behavior, and aggregate queries.

#include <unity.h>

#include "../../src/port_history.h"

namespace {

HistorySample mk(uint16_t v_mV, uint16_t i_mA, uint8_t proto = 0,
                 bool attached = true) {
  HistorySample s{};
  s.v_mV = v_mV;
  s.i_mA = i_mA;
  s.proto = proto;
  s.flags = attached ? 0x01 : 0x00;
  s.reserved = 0;
  return s;
}

}  // namespace

void test_empty(void) {
  PortHistory h;
  TEST_ASSERT_EQUAL_UINT32(0u, h.size());
  TEST_ASSERT_EQUAL_UINT16(0, h.avg_i_mA(10));
  uint32_t lo = 12345, hi = 12345;
  h.power_range_mW(10, lo, hi);
  TEST_ASSERT_EQUAL_UINT32(0u, lo);
  TEST_ASSERT_EQUAL_UINT32(0u, hi);
}

void test_single_push(void) {
  PortHistory h;
  h.push(mk(5000, 480));
  TEST_ASSERT_EQUAL_UINT32(1u, h.size());
  TEST_ASSERT_EQUAL_UINT16(5000, h.at(0).v_mV);
  TEST_ASSERT_EQUAL_UINT16(480, h.at(0).i_mA);
  TEST_ASSERT_EQUAL_UINT16(480, h.avg_i_mA(10));
}

void test_newest_first_order(void) {
  PortHistory h;
  for (uint16_t i = 1; i <= 5; ++i) h.push(mk(1000 * i, 100 * i));
  TEST_ASSERT_EQUAL_UINT32(5u, h.size());
  TEST_ASSERT_EQUAL_UINT16(5000, h.at(0).v_mV);
  TEST_ASSERT_EQUAL_UINT16(4000, h.at(1).v_mV);
  TEST_ASSERT_EQUAL_UINT16(1000, h.at(4).v_mV);
}

void test_capacity_minus_one(void) {
  PortHistory h;
  for (size_t i = 0; i < PortHistory::kCapacity - 1; ++i) {
    h.push(mk(static_cast<uint16_t>(i + 1), 0));
  }
  TEST_ASSERT_EQUAL_UINT32(PortHistory::kCapacity - 1, h.size());
  TEST_ASSERT_EQUAL_UINT16(static_cast<uint16_t>(PortHistory::kCapacity - 1),
                           h.at(0).v_mV);
}

void test_exact_capacity(void) {
  PortHistory h;
  for (size_t i = 0; i < PortHistory::kCapacity; ++i) {
    h.push(mk(static_cast<uint16_t>(i + 1), 0));
  }
  TEST_ASSERT_EQUAL_UINT32(PortHistory::kCapacity, h.size());
  TEST_ASSERT_EQUAL_UINT16(static_cast<uint16_t>(PortHistory::kCapacity),
                           h.at(0).v_mV);
  TEST_ASSERT_EQUAL_UINT16(1, h.at(PortHistory::kCapacity - 1).v_mV);
}

void test_wrap_drops_oldest(void) {
  PortHistory h;
  for (size_t i = 0; i < PortHistory::kCapacity + 5; ++i) {
    h.push(mk(static_cast<uint16_t>(i + 1), 0));
  }
  TEST_ASSERT_EQUAL_UINT32(PortHistory::kCapacity, h.size());
  // Newest is the last value pushed: kCapacity + 5.
  TEST_ASSERT_EQUAL_UINT16(static_cast<uint16_t>(PortHistory::kCapacity + 5),
                           h.at(0).v_mV);
  // Oldest still in buffer should be #6 (since 1..5 fell off).
  TEST_ASSERT_EQUAL_UINT16(6, h.at(PortHistory::kCapacity - 1).v_mV);
}

void test_avg_recent(void) {
  PortHistory h;
  for (uint16_t i = 0; i < 10; ++i) h.push(mk(0, static_cast<uint16_t>(100)));
  for (uint16_t i = 0; i < 5; ++i)  h.push(mk(0, static_cast<uint16_t>(200)));
  // Last 5 samples are 200, last 10 are 200,200,200,200,200,100,100,100,100,100
  TEST_ASSERT_EQUAL_UINT16(200, h.avg_i_mA(5));
  TEST_ASSERT_EQUAL_UINT16(150, h.avg_i_mA(10));
  // Asking for more than available falls back to whatever exists:
  // 5 samples at 200 plus 10 at 100 over all 15 -> (1000+1000)/15 = 133.
  TEST_ASSERT_EQUAL_UINT16(133, h.avg_i_mA(100));
}

void test_power_range(void) {
  PortHistory h;
  // 5V * 1A = 5W = 5000 mW
  h.push(mk(5000, 1000));
  // 9V * 2A = 18W = 18000 mW
  h.push(mk(9000, 2000));
  // 5V * 0.5A = 2.5W = 2500 mW
  h.push(mk(5000, 500));
  uint32_t lo = 0, hi = 0;
  h.power_range_mW(10, lo, hi);
  TEST_ASSERT_EQUAL_UINT32(2500u, lo);
  TEST_ASSERT_EQUAL_UINT32(18000u, hi);
}

void setUp(void)    {}
void tearDown(void) {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_empty);
  RUN_TEST(test_single_push);
  RUN_TEST(test_newest_first_order);
  RUN_TEST(test_capacity_minus_one);
  RUN_TEST(test_exact_capacity);
  RUN_TEST(test_wrap_drops_oldest);
  RUN_TEST(test_avg_recent);
  RUN_TEST(test_power_range);
  return UNITY_END();
}
