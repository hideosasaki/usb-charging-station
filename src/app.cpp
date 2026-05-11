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
constexpr uint32_t kLedTickMs     = 10;       // 100 Hz refresh: keeps dither above flicker fusion
constexpr uint32_t kHueCycleMs    = 10000;    // hue sweep period (full wheel)
constexpr uint32_t kErrBlinkMs    = 1000;     // I²C-error blink period (500 ms on, 500 ms off)
constexpr uint8_t  kPorts         = 3;
constexpr uint8_t  kRailsPerPort  = 2;

constexpr Rail kRails[kRailsPerPort] = {Rail::UsbC, Rail::UsbA};

Adafruit_NeoPixel pixel(kWs2812Count, kWs2812Pin, NEO_GRB + NEO_KHZ800);

PortReader*  readers[kPorts] = {nullptr, nullptr, nullptr};
PortHistory  history[kPorts];                       // Vbus is shared per port
SessionStats session[kPorts][kRailsPerPort]{};      // indexed by [port][Rail]
DisplayUi    ui;

constexpr uint8_t kLedMax = 1;      // 8-bit minimum; dimmest light WS2812B can emit
uint32_t last_sample_ms = 0;
uint32_t last_led_ms    = 0;
bool     g_led_err      = false;     // latest sample saw any I²C fault
int16_t  g_err_r = 0, g_err_g = 0, g_err_b = 0;
uint32_t g_last_rgb = 0xFFFFFFFFu;   // sentinel forces first paint

// At brightness 1, mixed colors (e.g. yellow = R+G half-on) would round
// to a single primary or to zero, so each channel is dithered
// temporally at 100 Hz: target intensity lives in [0, 2] and a
// Bresenham accumulator decides "0 or 1" each tick. An I²C fault swaps
// in a red 1 Hz square-wave so the failure mode is obvious even if the
// panel is dark.
void heartbeat_tick(uint32_t now) {
  uint8_t r = 0, g = 0, b = 0;
  if (g_led_err) {
    bool on = ((now / (kErrBlinkMs / 2)) & 1u) == 0;
    r = on ? kLedMax : 0;
  } else {
    uint16_t hue = (uint16_t)((now % kHueCycleMs) * 65535u / kHueCycleMs);
    uint32_t raw = pixel.ColorHSV(hue, 255, (uint8_t)(kLedMax * 2));
    int16_t tr = (int16_t)((raw >> 16) & 0xFF);
    int16_t tg = (int16_t)((raw >>  8) & 0xFF);
    int16_t tb = (int16_t)( raw        & 0xFF);
    constexpr int16_t step = (int16_t)kLedMax;
    constexpr int16_t span = step * 2;
    auto dither = [](int16_t target, int16_t& err) -> uint8_t {
      err += target;
      if (err >= span) { err -= span; return (uint8_t)step; }
      return 0;
    };
    r = dither(tr, g_err_r);
    g = dither(tg, g_err_g);
    b = dither(tb, g_err_b);
  }
  uint32_t rgb = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
  if (rgb == g_last_rgb) return;     // skip the bit-bang when unchanged
  g_last_rgb = rgb;
  pixel.setPixelColor(0, pixel.Color(r, g, b));
  pixel.show();
}

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
  pixel.show();    // initial state: off; heartbeat_tick paints from now on

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
  if (now - last_led_ms >= kLedTickMs) {
    last_led_ms = now;
    heartbeat_tick(now);
  }
  if (now - last_sample_ms < kSampleMs) return;
  last_sample_ms = now;

  bool any_err = false;
  PortSnapshot snap[kPorts];
  uint32_t     total_mW = 0;
  for (uint8_t i = 0; i < kPorts; ++i) {
    PortReading r = readers[i] ? readers[i]->read(now) : PortReading{};
    history[i].push(to_sample(r));
    if (r.is_fault()) any_err = true;

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
  g_led_err = any_err;
}
