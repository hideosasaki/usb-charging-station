// Liveness signal: if the WS2812B keeps cycling but the display stays
// blank, the MCU is alive and the fault is on the SPI/display side.

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#include "display.h"

constexpr uint8_t  WS2812_PIN   = 16;
constexpr uint16_t WS2812_COUNT = 1;
constexpr uint32_t HEARTBEAT_MS = 1000;

constexpr uint32_t kHeartbeat[6] = {
    0xFF0000, 0, 0x00FF00, 0, 0x0000FF, 0,
};

Adafruit_NeoPixel pixel(WS2812_COUNT, WS2812_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  Serial.begin(115200);
  // Brief wait so the first prints land if a host is already attached;
  // CDC enumerates fine without it, so don't block the user for 3s.
  uint32_t t0 = millis();
  while (!Serial && (millis() - t0) < 500) { delay(10); }

  pixel.begin();
  pixel.setBrightness(32);
  pixel.show();

  Serial.println();
  Serial.println(F("=== USB Charging Station boot ==="));
  Serial.print(F("Built: "));
  Serial.print(F(__DATE__));
  Serial.print(F(" "));
  Serial.println(F(__TIME__));

  display_begin();
  display_hello();
  Serial.println(F("display_hello() done"));
}

void loop() {
  static uint8_t  phase = 0;
  static uint32_t last  = 0;
  uint32_t now = millis();
  if (now - last < HEARTBEAT_MS) return;
  last = now;

  pixel.setPixelColor(0, kHeartbeat[phase % 6]);
  pixel.show();

  Serial.print(F("tick "));
  Serial.print(phase);
  Serial.print(F("  uptime="));
  Serial.print(now / 1000);
  Serial.println(F("s"));
  phase = (phase + 1) % 6;
}
