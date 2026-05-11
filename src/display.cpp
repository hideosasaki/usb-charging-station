// ILI9341 driver wrapper. Owns the single TFT_eSPI instance so the type
// never escapes through display.h.

#include "display.h"

#include <Arduino.h>
#include <TFT_eSPI.h>

namespace {

constexpr uint8_t  BL_PIN          = 15;
constexpr uint32_t BL_PWM_HZ       = 20000;   // above audible range
constexpr uint8_t  BL_TARGET       = 200;     // ~78% of 8-bit range
constexpr uint16_t BL_FADE_MS      = 300;
constexpr uint8_t  BL_FADE_STEPS   = 20;

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
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  backlight_fade_in();
}

void display_set_backlight(uint8_t pwm) {
  analogWrite(BL_PIN, pwm);
}

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
