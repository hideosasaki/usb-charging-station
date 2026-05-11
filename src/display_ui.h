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
    uint8_t  proto;
    uint8_t  phase;
    bool     attached;
    bool     valid;
  };

  Cache    last_[3]{};
  uint32_t last_total_mW_  = 0xFFFFFFFFu;
  uint32_t last_elapsed_s_ = 0xFFFFFFFFu;
};
