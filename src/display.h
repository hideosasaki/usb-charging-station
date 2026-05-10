// ILI9341 display API. The TFT_eSPI instance is owned in display.cpp;
// callers stay decoupled from TFT_eSPI types.

#pragma once

#include <stdint.h>

void display_begin();
void display_hello();
void display_set_backlight(uint8_t pwm);
