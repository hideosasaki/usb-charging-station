// SW3518 PortReader, active only when USE_MOCK_PORTS=0. The chip
// address is fixed at 0x3C, so each chip needs its own I2C bus.

#include "build_config.h"

#if !USE_MOCK_PORTS

#include "sw3518_port_reader.h"

#include <Arduino.h>
#include <Wire.h>

#include "wire_i2c_wrapper.h"

namespace {

using SW35xx_lib::SW35xx;

constexpr uint16_t kAttachedVThreshold_mV = 4500;
constexpr uint16_t kAttachedIThreshold_mA = 50;

Protocol map_protocol(SW35xx::FastChargeInfo info, bool attached,
                      uint16_t vout_mV) {
  if (!attached) return Protocol::None;
  switch (info.protocol) {
    case SW35xx::QC2:    return Protocol::Qc20;
    case SW35xx::QC3:    return Protocol::Qc30;
    case SW35xx::PD_FIX:
      return (info.pdVersion >= 3) ? Protocol::Pd30 : Protocol::Pd20;
    case SW35xx::PD_PPS: return Protocol::Pd31Pps;
    // FCP/SCP/MTKPE/LVDC/SFCP/AFC are proprietary fast-charge schemes
    // we don't model. Treat them as plain 5V if the rail looks like
    // it, otherwise hide them behind PD2.0 — the V/I numbers tell the
    // user more than an unfamiliar protocol name would.
    case SW35xx::NOT_FAST_CHARGE:
    default:
      return (vout_mV >= 4500 && vout_mV <= 5500) ? Protocol::Std5V
                                                  : Protocol::Pd20;
  }
}

}  // namespace

Sw3518PortReader::Sw3518PortReader(uint8_t idx, I2CInterface& bus)
    : idx_(idx), bus_(bus), chip_(bus_) {}

bool Sw3518PortReader::begin() {
  bus_.begin();
  chip_.begin();
  // 0xFF means I2C read returned its error sentinel. Anything in
  // 0..7 (chip version field) counts as a live SW3518.
  uint8_t ver = chip_.getChipVersion();
  probed_ok_  = (ver != 0xFF);
  if (probed_ok_) last_ok_ms_ = millis();
  return probed_ok_;
}

PortReading Sw3518PortReader::read(uint32_t now_ms) {
  PortReading r{};
  r.t_ms = now_ms;

  if (!probed_ok_) {
    r.err = PortError::I2cNack;
    return r;
  }

  chip_.readStatus();
  SW35xx::FastChargeInfo fc = chip_.getFastChargeInfo();

  uint16_t v_mV   = chip_.vout_mV;
  uint16_t i_c_mA = chip_.iout_usbc_mA;
  uint16_t i_a_mA = chip_.iout_usba_mA;

  uint8_t mask = 0;
  // Vbus is shared, so a healthy 5V+ rail with no measurable current is
  // ambiguous between the two connectors. Bias toward USB-C, which is the
  // negotiable side; the A flag fires only when actual current is seen.
  if (v_mV >= kAttachedVThreshold_mV || i_c_mA >= kAttachedIThreshold_mA) {
    mask |= kRailMaskC;
  }
  if (i_a_mA >= kAttachedIThreshold_mA) {
    mask |= kRailMaskA;
  }

  r.v_mV      = v_mV;
  r.i_c_mA    = i_c_mA;
  r.i_a_mA    = i_a_mA;
  r.rail_mask = mask;
  r.attached  = (mask != 0);
  r.proto     = map_protocol(fc, (mask & kRailMaskC) != 0, v_mV);

  // The vendored driver returns 0 on I2C read error, so a dead bus
  // looks identical to an idle port. Best we can do: assume the bus
  // is healthy as long as we see *some* non-zero report within the
  // window, and surface Stale otherwise.
  bool any_activity = (v_mV != 0) || (i_c_mA != 0) || (i_a_mA != 0) ||
                      fc.ledOn || fc.protocol != SW35xx::NOT_FAST_CHARGE;
  if (any_activity) {
    last_ok_ms_ = now_ms;
  } else if ((now_ms - last_ok_ms_) > 5000) {
    r.err = PortError::Stale;
  }
  return r;
}

namespace {

// HW I2C #0 on GP0=SDA, GP1=SCL.
WireI2CWrapper      g_bus0(Wire, /*sda=*/0, /*scl=*/1, /*clock_hz=*/100000);
Sw3518PortReader    g_p0(0, g_bus0);

}  // namespace

PortReader* make_port_reader(uint8_t idx) {
  if (idx >= SW3518_PORT_COUNT) return nullptr;
  switch (idx) {
    case 0: return &g_p0;
    default: return nullptr;
  }
}

#endif  // !USE_MOCK_PORTS
