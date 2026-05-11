// ILI9341 display API. The TFT_eSPI instance is owned in display.cpp;
// callers stay decoupled from TFT_eSPI types.

#pragma once

#include <stdint.h>

class TFT_eSPI;

void display_begin();
void display_hello();
void display_set_backlight(uint8_t pwm);

// View code includes <TFT_eSPI.h> to use the returned reference; other
// translation units stay decoupled because only the forward declaration
// is visible here.
TFT_eSPI& display_tft();
