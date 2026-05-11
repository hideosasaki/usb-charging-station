// Controller: pulls a sample from each PortReader once per second, feeds
// it into the model layer, then hands snapshots to the View.

#include "app.h"

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#include "charge_analyzer.h"
#include "display.h"
#include "display_ui.h"
#include "port_history.h"
#include "port_reader.h"
#include "session_stats.h"

namespace {

constexpr uint8_t  kWs2812Pin    = 16;
constexpr uint16_t kWs2812Count  = 1;
constexpr uint32_t kSampleMs     = 1000;

constexpr uint32_t kHeartbeat[6] = {
    0xFF0000, 0, 0x00FF00, 0, 0x0000FF, 0,
};

Adafruit_NeoPixel pixel(kWs2812Count, kWs2812Pin, NEO_GRB + NEO_KHZ800);

PortReader*  readers[3] = {nullptr, nullptr, nullptr};
PortHistory  history[3];
SessionStats session[3]{};
DisplayUi    ui;

uint32_t last_sample_ms  = 0;
uint8_t  heartbeat_phase = 0;

HistorySample to_sample(const PortReading& r) {
  HistorySample s{};
  s.v_mV  = r.v_mV;
  s.i_mA  = r.i_mA;
  s.proto = (uint8_t)r.proto;
  s.flags = (r.attached ? kFlagAttached : 0) |
            (r.err != PortError::Ok ? kFlagError : 0);
  return s;
}

}  // namespace

void app_setup() {
  Serial.begin(115200);
  uint32_t t0 = millis();
  while (!Serial && (millis() - t0) < 500) { delay(10); }

  pixel.begin();
  pixel.setBrightness(32);
  pixel.show();

  Serial.println();
  Serial.println(F("=== USB Charging Station boot ==="));

  display_begin();
  display_hello();
  delay(800);

  for (uint8_t i = 0; i < 3; ++i) {
    readers[i] = make_port_reader(i);
    if (readers[i]) readers[i]->begin();
    session_reset(session[i]);
  }
  ui.begin();
  Serial.println(F("app ready"));
}

void app_loop() {
  uint32_t now = millis();
  if (now - last_sample_ms < kSampleMs) return;
  last_sample_ms = now;

  PortSnapshot snap[3];
  uint32_t     total_mW = 0;
  for (uint8_t i = 0; i < 3; ++i) {
    PortReading r = readers[i] ? readers[i]->read(now) : PortReading{};
    history[i].push(to_sample(r));
    session_update(session[i], r, kSampleMs);

    snap[i].live    = r;
    snap[i].phase   = analyze(history[i], r);
    snap[i].session = session[i];
    snap[i].history = &history[i];

    if (r.attached) total_mW += power_mW(r.v_mV, r.i_mA);
  }
  ui.render(snap, total_mW, now);

  pixel.setPixelColor(0, kHeartbeat[heartbeat_phase]);
  pixel.show();
  heartbeat_phase = (heartbeat_phase + 1) % 6;
}
