// Controller: pulls a sample from each PortReader once per second, feeds
// it into the model layer, then hands snapshots to the View.

#include "app.h"

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#include "build_config.h"
#include "charge_analyzer.h"
#include "display.h"
#include "display_ui.h"
#include "port_bridge.h"
#include "port_history.h"
#include "port_reader.h"
#include "session_stats.h"

#if USE_MOCK_PORTS
#include "mock_port_reader.h"
#include "serial_cmd.h"
#endif

namespace {

constexpr uint8_t  kWs2812Pin     = 16;
constexpr uint16_t kWs2812Count   = 1;
constexpr uint32_t kSampleMs      = 1000;
constexpr uint8_t  kPorts         = 3;
constexpr uint8_t  kRailsPerPort  = 2;

constexpr uint32_t kHeartbeat[6] = {
    0xFF0000, 0, 0x00FF00, 0, 0x0000FF, 0,
};

constexpr Rail kRails[kRailsPerPort] = {Rail::UsbC, Rail::UsbA};

Adafruit_NeoPixel pixel(kWs2812Count, kWs2812Pin, NEO_GRB + NEO_KHZ800);

PortReader*  readers[kPorts] = {nullptr, nullptr, nullptr};
PortHistory  history[kPorts];                       // Vbus is shared per port
SessionStats session[kPorts][kRailsPerPort]{};      // indexed by [port][Rail]
DisplayUi    ui;

uint32_t last_sample_ms  = 0;
uint8_t  heartbeat_phase = 0;

const char* err_name(PortError e) {
  switch (e) {
    case PortError::Ok:         return "OK";
    case PortError::NotPresent: return "NoDev";
    case PortError::I2cTimeout: return "Timeout";
    case PortError::I2cNack:    return "Nack";
    case PortError::Stale:      return "Stale";
  }
  return "?";
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

  for (uint8_t i = 0; i < kPorts; ++i) {
    readers[i] = make_port_reader(i);
    if (readers[i]) readers[i]->begin();
    for (Rail rail : kRails) session_reset(session[i][(uint8_t)rail]);
  }
  ui.begin();
#if USE_MOCK_PORTS
  static MockPortReader* mocks[kPorts] = {make_mock_port_reader(0),
                                          make_mock_port_reader(1),
                                          make_mock_port_reader(2)};
  serial_cmd_init(mocks);
#endif
  Serial.println(F("app ready"));
}

void app_loop() {
#if USE_MOCK_PORTS
  serial_cmd_poll();
#endif

  uint32_t now = millis();
  if (now - last_sample_ms < kSampleMs) return;
  last_sample_ms = now;

  PortSnapshot snap[kPorts];
  uint32_t     total_mW = 0;
  for (uint8_t i = 0; i < kPorts; ++i) {
    PortReading r = readers[i] ? readers[i]->read(now) : PortReading{};
    history[i].push(to_sample(r));

    for (Rail rail : kRails) {
      uint8_t idx = (uint8_t)rail;
      session_update(session[i][idx], r, rail, kSampleMs);
      snap[i].phase[idx]   = analyze(history[i], rail, r);
      snap[i].session[idx] = session[i][idx];
    }
    snap[i].live    = r;
    snap[i].history = &history[i];

    if (r.attached()) total_mW += power_mW(r.v_mV, r.total_i_mA());

    // Skip the trace when nothing is plugged in or when no host is
    // reading from CDC, both to keep the log signal high and to avoid
    // blocking on a full USB TX buffer with no consumer.
    if (Serial && (r.attached() || r.err != PortError::Ok)) {
      const char* rails = r.has_c() ? (r.has_a() ? "C+A" : "C")
                                    : (r.has_a() ? "A" : "-");
      Serial.printf("[t=%lus] port%u: rails=%s V=%umV Ic=%umA Ia=%umA proto=%s err=%s\n",
                    (unsigned long)(now / 1000), i,
                    rails, r.v_mV, r.i_c_mA, r.i_a_mA,
                    protocol_name(r.proto), err_name(r.err));
    }
  }
  ui.render(snap, total_mW, now);

  pixel.setPixelColor(0, kHeartbeat[heartbeat_phase]);
  pixel.show();
  heartbeat_phase = (heartbeat_phase + 1) % 6;
}
