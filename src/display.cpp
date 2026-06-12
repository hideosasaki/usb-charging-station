// ILI9341 driver wrapper. Owns the single TFT_eSPI instance so the type
// never escapes through display.h.

#include "display.h"

#include <Arduino.h>
#include <TFT_eSPI.h>

namespace {

constexpr uint8_t  BL_PIN          = 28;
constexpr uint32_t BL_PWM_HZ       = 20000;   // above audible range
constexpr uint8_t  BL_TARGET       = 26;      // ~10% duty (~40 mA total)
constexpr uint16_t BL_FADE_MS      = 300;
constexpr uint8_t  BL_FADE_STEPS   = 20;

constexpr uint8_t  ROTATION        = 1;       // landscape, 320x240

// Blind-repair pacing. The panel is write-only (MISO and RST are not
// wired), so a dropped connection cannot be detected; instead a small
// set of idempotent config commands is re-sent on a fixed period. On a
// healthy panel they change nothing visible; a panel that lost power
// (back at its power-on defaults) is reconfigured within one period.
constexpr uint32_t REPAIR_PERIOD_MS   = 3000;
constexpr uint32_t REPAIR_WAKE_GAP_MS = 10;   // SLPOUT needs 5 ms before the next command

TFT_eSPI tft;

void backlight_fade_in() {
  for (uint8_t i = 0; i <= BL_FADE_STEPS; ++i) {
    uint8_t pwm = (uint16_t)BL_TARGET * i / BL_FADE_STEPS;
    analogWrite(BL_PIN, pwm);
    delay(BL_FADE_MS / BL_FADE_STEPS);
  }
}

}  // namespace

void display_begin() {
  pinMode(BL_PIN, OUTPUT);
  analogWriteFreq(BL_PWM_HZ);
  analogWrite(BL_PIN, 0);

  tft.init();
  tft.setRotation(ROTATION);
  tft.fillScreen(TFT_BLACK);

  backlight_fade_in();
}

bool display_repair_tick(uint32_t now_ms) {
  static uint32_t last_repair_ms = 0;
  static uint32_t wake_ms        = 0;
  static bool     awaiting_apply = false;

  if (!awaiting_apply) {
    if (now_ms - last_repair_ms < REPAIR_PERIOD_MS) return false;
    tft.writecommand(0x11);            // SLPOUT
    wake_ms        = now_ms;
    awaiting_apply = true;
    return false;
  }
  if (now_ms - wake_ms < REPAIR_WAKE_GAP_MS) return false;
  tft.setRotation(ROTATION);           // re-sends MADCTL
  tft.writecommand(0x3A);              // COLMOD
  tft.writedata(0x55);                 // 16-bit RGB565
  tft.writecommand(0x29);              // DISPON
  awaiting_apply = false;
  last_repair_ms = now_ms;
  return true;
}

void display_set_backlight(uint8_t pwm) {
  analogWrite(BL_PIN, pwm);
}

TFT_eSPI& display_tft() { return tft; }

void display_hello() {
  const int16_t w = tft.width();
  const int16_t h = tft.height();

  // RGB bars reveal RGB-vs-BGR ordering issues at first boot.
  const int16_t bar_w = w / 4;
  tft.fillRect(0 * bar_w, 0, bar_w, h, TFT_RED);
  tft.fillRect(1 * bar_w, 0, bar_w, h, TFT_GREEN);
  tft.fillRect(2 * bar_w, 0, bar_w, h, TFT_BLUE);
  tft.fillRect(3 * bar_w, 0, w - 3 * bar_w, h, TFT_WHITE);

  delay(800);
  tft.fillScreen(TFT_BLACK);

  // Top-left origin marker confirms rotation/origin.
  tft.fillRect(0, 0, 5, 5, TFT_YELLOW);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("USB Charging Station", 10, 10, 4);
  tft.drawString("Built: " __DATE__ " " __TIME__, 10, 50, 2);

  tft.drawCircle(w - 40, h - 40, 25, TFT_CYAN);
  tft.drawLine(10, h - 10, w - 10, h - 10, TFT_MAGENTA);
}
