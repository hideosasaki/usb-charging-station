// USB Charging Station - Phase 0: Blink + Serial sanity check
// Target: Waveshare RP2040-Zero (Earle Philhower core)

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

constexpr uint8_t  WS2812_PIN      = 16;
constexpr uint16_t WS2812_COUNT    = 1;
constexpr uint32_t BLINK_PERIOD_MS = 1000;

Adafruit_NeoPixel pixel(WS2812_COUNT, WS2812_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  Serial.begin(115200);
  uint32_t t0 = millis();
  while (!Serial && (millis() - t0) < 3000) { delay(10); }

  pixel.begin();
  pixel.setBrightness(32);
  pixel.show();

  Serial.println();
  Serial.println(F("=== USB Charging Station - Phase 0 ==="));
  Serial.print(F("Built: "));
  Serial.print(F(__DATE__));
  Serial.print(F(" "));
  Serial.println(F(__TIME__));
  Serial.println(F("WS2812B blink on GP16, 1Hz, R/G/B cycle"));
}

void loop() {
  static uint8_t  phase = 0;
  static uint32_t last  = 0;
  uint32_t now = millis();
  if (now - last < BLINK_PERIOD_MS) return;
  last = now;

  switch (phase % 6) {
    case 0: pixel.setPixelColor(0, pixel.Color(255,   0,   0)); break;
    case 1: pixel.setPixelColor(0, pixel.Color(  0,   0,   0)); break;
    case 2: pixel.setPixelColor(0, pixel.Color(  0, 255,   0)); break;
    case 3: pixel.setPixelColor(0, pixel.Color(  0,   0,   0)); break;
    case 4: pixel.setPixelColor(0, pixel.Color(  0,   0, 255)); break;
    case 5: pixel.setPixelColor(0, pixel.Color(  0,   0,   0)); break;
  }
  pixel.show();

  Serial.print(F("tick "));
  Serial.print(phase);
  Serial.print(F("  uptime="));
  Serial.print(now / 1000);
  Serial.println(F("s"));
  phase++;
}
