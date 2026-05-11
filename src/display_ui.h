// 3-port live view. Holds a last-drawn cache so render() only repaints
// fields whose values changed since the previous tick.

#pragma once

#include <stdint.h>

#include "charge_analyzer.h"
#include "port_history.h"
#include "port_reader.h"
#include "session_stats.h"

struct PortSnapshot {
  PortReading        live;
  Phase              phase;
  SessionStats       session;
  const PortHistory* history;
};

class DisplayUi {
 public:
  void begin();
  void render(const PortSnapshot (&ports)[3], uint32_t total_mW,
              uint32_t now_ms);

 private:
  struct Cache {
    uint16_t v_mV;
    uint16_t i_mA;
    uint32_t w_mW;
    uint32_t elapsed_s;
    uint32_t energy_cWh;     // centi-Wh, matches the displayed precision
    uint32_t eta_s;
    uint8_t  proto;
    uint8_t  phase;
    uint8_t  progress_pct;
    bool     progress_valid;
    bool     eta_valid;
    bool     attached;
    bool     valid;
  };

  Cache    last_[3]{};
  uint32_t last_total_mW_ = 0xFFFFFFFFu;
};
