// Scenario-driven PortReader implementation. Scenarios are pure
// functions of now_ms; jitter and overrides are layered on top.

#include "mock_port_reader.h"

#include "build_config.h"

#if USE_MOCK_PORTS

namespace {

// Scenario sample: Vbus is shared, so v_mV is one number, but currents
// are per-rail to match the real chip. Use the helpers below to build
// one — they apply a single rule for which rails are attached so
// scenarios never carry a hand-written rail_mask.
struct Nominal {
  uint16_t v_mV;
  uint16_t i_c_mA;
  uint16_t i_a_mA;
  Protocol proto;     // chip-wide, like the real SW3518 indicator
};

Nominal single_c(uint16_t v_mV, uint16_t i_mA, Protocol proto) {
  return Nominal{v_mV, i_mA, 0, proto};
}

Nominal dual_ca(uint16_t v_mV, uint16_t i_c_mA, uint16_t i_a_mA,
                Protocol proto) {
  return Nominal{v_mV, i_c_mA, i_a_mA, proto};
}

Nominal single_a(uint16_t v_mV, uint16_t i_mA, Protocol proto) {
  return Nominal{v_mV, 0, i_mA, proto};
}

uint16_t lerp_u16(uint32_t t, uint32_t t0, uint32_t t1, uint16_t v0,
                  uint16_t v1) {
  if (t <= t0) return v0;
  if (t >= t1) return v1;
  uint32_t span = t1 - t0;
  int32_t  dv   = (int32_t)v1 - (int32_t)v0;
  return (uint16_t)((int32_t)v0 + (dv * (int32_t)(t - t0)) / (int32_t)span);
}

Nominal scenario_a(uint32_t now_ms) {
  uint32_t t = now_ms % 740'000u;
  if (t < 2'000u)   return Nominal{};
  if (t < 5'000u)   return single_c(5000, 300,  Protocol::Std5V);
  if (t < 305'000u) return single_c(9000, 2050, Protocol::Pd30);
  if (t < 605'000u) {
    uint16_t i = lerp_u16(t, 305'000u, 605'000u, 2050, 300);
    return single_c(9000, i, Protocol::Pd30);
  }
  if (t < 720'000u) return single_c(5000, 300, Protocol::Pd30);
  return Nominal{};
}

// 0..300s: PD3.0 9V/2A on USB-C alone.
// 300..600s: both rails active, Vbus clamped to 5V (the SW3518 dual-port
//   limit), Ic=1500 mA and Ia=800 mA.
// 600..900s: USB-C unplugged, USB-A alone at 5V/1000 mA.
// Loops at 900s so soak tests can run unattended.
Nominal scenario_b(uint32_t now_ms) {
  uint32_t t = now_ms % 900'000u;
  if (t < 300'000u) return single_c(9000, 2050, Protocol::Pd30);
  if (t < 600'000u) return dual_ca(5000, 1500, 800, Protocol::Std5V);
  return single_a(5000, 1000, Protocol::Std5V);
}

Nominal scenario_c(uint32_t now_ms) {
  uint32_t t = now_ms % 180'000u;
  if (t >= 120'000u) return single_c(12000, 1500, Protocol::Qc30);
  return Nominal{};
}

Nominal nominal_for(ScenarioId s, uint32_t now_ms) {
  switch (s) {
    case ScenarioId::A_Pd30Phone: return scenario_a(now_ms);
    case ScenarioId::B_DualCpA:   return scenario_b(now_ms);
    case ScenarioId::C_IdleBurst: return scenario_c(now_ms);
  }
  return Nominal{};
}

uint16_t apply_jitter(uint16_t i_mA, int16_t permille) {
  if (i_mA == 0) return 0;
  int32_t scaled = (int32_t)i_mA * (1000 + permille) / 1000;
  if (scaled < 0) return 0;
  if (scaled > 0xFFFF) return 0xFFFF;
  return (uint16_t)scaled;
}

}  // namespace

MockPortReader::MockPortReader(uint8_t idx, ScenarioId scenario, uint32_t seed)
    : idx_(idx), scenario_(scenario), rng_(seed) {}

bool MockPortReader::begin() { return true; }

int16_t MockPortReader::jitter_permille() {
  return (int16_t)((int)(rng_() % 41u) - 20);
}

PortReading MockPortReader::sample_scenario(uint32_t now_ms) const {
  Nominal n = nominal_for(scenario_, now_ms);
  PortReading r{};
  r.t_ms   = now_ms;
  r.v_mV   = n.v_mV;
  r.i_c_mA = n.i_c_mA;
  r.i_a_mA = n.i_a_mA;
  r.proto  = n.proto;
  r.err    = PortError::Ok;
  // A rail is "attached" when current flows through it. v_mV is shared,
  // so attaching off voltage alone would force both rails on.
  r.set_rail(Rail::UsbC, n.i_c_mA > 0);
  r.set_rail(Rail::UsbA, n.i_a_mA > 0);
  return r;
}

PortReading MockPortReader::read(uint32_t now_ms) {
  PortReading r{};
  if (has_override_) {
    r.t_ms   = now_ms;
    r.v_mV   = ov_v_mV_;
    r.i_c_mA = ov_i_c_mA_;
    r.i_a_mA = ov_i_a_mA_;
    r.proto  = ov_proto_;
    r.err    = PortError::Ok;
    // Override is authoritative — only the explicit currents decide
    // which rails are attached.
    r.set_rail(Rail::UsbC, ov_i_c_mA_ > 0);
    r.set_rail(Rail::UsbA, ov_i_a_mA_ > 0);
  } else {
    r = sample_scenario(now_ms);
    if (force_ == Force::Detach) {
      r.clear_rails();
      r.v_mV = r.i_c_mA = r.i_a_mA = 0;
      r.proto = Protocol::None;
    } else {
      if (force_ == Force::Attach && !r.attached()) {
        r.v_mV   = 5000;
        r.i_c_mA = 300;
        r.proto  = Protocol::Std5V;
        r.set_rail(Rail::UsbC, true);
      }
      if (r.attached()) {
        // Shared jitter draw keeps both rails moving in step; the RNG
        // stream is still per-port.
        int16_t j = jitter_permille();
        r.i_c_mA = apply_jitter(r.i_c_mA, j);
        r.i_a_mA = apply_jitter(r.i_a_mA, j);
      }
    }
  }
  last_ = r;
  return r;
}

void MockPortReader::set_override(uint16_t v_mV, uint16_t i_c_mA,
                                  uint16_t i_a_mA, Protocol proto) {
  has_override_ = true;
  ov_v_mV_      = v_mV;
  ov_i_c_mA_    = i_c_mA;
  ov_i_a_mA_    = i_a_mA;
  ov_proto_     = proto;
}

void MockPortReader::clear_override()      { has_override_ = false; }
void MockPortReader::force_detach()        { force_ = Force::Detach; }
void MockPortReader::force_attach()        { force_ = Force::Attach; }
void MockPortReader::set_scenario(ScenarioId s) { scenario_ = s; force_ = Force::None; }
void MockPortReader::resume_auto()              { has_override_ = false; force_ = Force::None; }

// Per-port seeds keep the three jitter streams independent. File-scope
// so make_port_reader and make_mock_port_reader share the same instances.
static MockPortReader g_r0(0, ScenarioId::A_Pd30Phone,   0xA51C3001u);
static MockPortReader g_r1(1, ScenarioId::B_DualCpA,   0xA51C3002u);
static MockPortReader g_r2(2, ScenarioId::C_IdleBurst,   0xA51C3003u);

MockPortReader* make_mock_port_reader(uint8_t idx) {
  switch (idx) {
    case 0: return &g_r0;
    case 1: return &g_r1;
    case 2: return &g_r2;
  }
  return nullptr;
}

PortReader* make_port_reader(uint8_t idx) {
  return make_mock_port_reader(idx);
}

#endif  // USE_MOCK_PORTS
