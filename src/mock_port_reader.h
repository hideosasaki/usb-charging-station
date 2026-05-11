// Scenario-driven PortReader used until SW3518 hardware is wired up.
// A small LCG (minstd_rand, 8 bytes) overlays +/-2% jitter so per-port
// state stays cheap; mt19937 would burn ~2.5KB SRAM per port for no
// statistical benefit at this scale.

#pragma once

#include <stdint.h>

#include <random>

#include "port_reader.h"

enum class ScenarioId : uint8_t {
  A_Pd30Phone = 0,   // Port 0 default
  B_Std5VSteady = 1, // Port 1 default
  C_IdleBurst = 2,   // Port 2 default
};

class MockPortReader : public PortReader {
 public:
  MockPortReader(uint8_t idx, ScenarioId scenario, uint32_t seed);

  bool        begin() override;
  PortReading read(uint32_t now_ms) override;
  uint8_t     index() const override { return idx_; }

  // Runtime overrides used by the Serial command parser. set_override pins
  // the port to a fixed reading until clear_override() is called.
  void set_override(uint16_t v_mV, uint16_t i_mA, Protocol proto);
  void clear_override();
  void force_detach();
  void force_attach();
  void set_scenario(ScenarioId scenario);

 private:
  PortReading sample_scenario(uint32_t now_ms) const;
  int16_t     jitter_permille();

  uint8_t          idx_;
  ScenarioId       scenario_;
  std::minstd_rand rng_;

  bool        has_override_ = false;
  uint16_t    ov_v_mV_      = 0;
  uint16_t    ov_i_mA_      = 0;
  Protocol    ov_proto_     = Protocol::None;

  enum class Force : uint8_t { None, Detach, Attach };
  Force force_ = Force::None;
};
