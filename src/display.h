// ILI9341 display API. The TFT_eSPI instance is owned in display.cpp;
// callers stay decoupled from TFT_eSPI types.

#pragma once

#include <stdint.h>

class TFT_eSPI;

void display_begin();
void display_hello();
void display_set_backlight(uint8_t pwm);

// Periodic blind repair for flaky panel wiring. The panel is write-only,
// so a glitch cannot be detected; instead idempotent config commands
// (sleep-out, pixel format, rotation, display-on) are re-sent every few
// seconds. Call every loop tick with millis(); returns true when a
// repair cycle just completed and the caller should force a full
// repaint of the UI.
bool display_repair_tick(uint32_t now_ms);

// View code includes <TFT_eSPI.h> to use the returned reference; other
// translation units stay decoupled because only the forward declaration
// is visible here.
TFT_eSPI& display_tft();
