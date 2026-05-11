// 3-port View. Static frame is painted once in begin(); render() only
// repaints fields whose values changed since the previous tick.

#include "display_ui.h"

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <stdio.h>

#include "display.h"

namespace {

constexpr int16_t  kScreenW     = 320;
constexpr int16_t  kScreenH     = 240;
constexpr int16_t  kColW        = 100;
constexpr int16_t  kSummaryY    = 200;
constexpr int16_t  kElapsedX    = 180;
constexpr int16_t  kSummaryPadY = 8;
constexpr int16_t  kSummaryPadX = 6;
constexpr uint16_t kFrameColor  = 0x4208;  // dark grey
constexpr uint16_t kValueColor  = TFT_WHITE;
constexpr uint16_t kProtoColor  = TFT_CYAN;
constexpr uint16_t kBgColor     = TFT_BLACK;

constexpr int16_t kVoltageY    = 28;
constexpr int16_t kCurrentY    = 64;
constexpr int16_t kPowerY      = 100;
constexpr int16_t kProtoY      = 140;
constexpr int16_t kPhaseY      = 168;
constexpr int16_t kSparkY      = 186;
constexpr int16_t kSparkH      = 12;
constexpr int16_t kFieldW      = kColW - 12;
constexpr int16_t kSparkBins   = kFieldW;             // one px per bin
constexpr size_t  kSparkWindow = 3600;                // full 1h history
constexpr int16_t kBigFontH    = 26;
constexpr int16_t kSmallFontH  = 16;
constexpr uint16_t kSparkColor = TFT_GREEN;

uint16_t phase_color(Phase p) {
  switch (p) {
    case Phase::CC:       return TFT_YELLOW;
    case Phase::CV:       return TFT_GREEN;
    case Phase::NearDone: return TFT_SKYBLUE;
    case Phase::Done:     return TFT_DARKGREY;
    case Phase::Idle:     return TFT_DARKGREY;
  }
  return TFT_DARKGREY;
}

int16_t col_x(uint8_t i) { return i * kColW + 4; }

void draw_frame() {
  auto& t = display_tft();
  t.fillScreen(kBgColor);
  for (uint8_t i = 0; i < 3; ++i) {
    int16_t x = i * kColW;
    t.drawFastVLine(x, 0, kSummaryY, kFrameColor);
    char hdr[8];
    snprintf(hdr, sizeof(hdr), "P%u", (unsigned)i);
    t.setTextColor(TFT_WHITE, kBgColor);
    t.drawString(hdr, x + 6, 4, 2);
  }
  t.drawFastVLine(3 * kColW, 0, kSummaryY, kFrameColor);
  t.drawFastHLine(0, kSummaryY, kScreenW, kFrameColor);
}

void draw_value(uint8_t col, int16_t y, int16_t h, uint16_t fg,
                const char* text, uint8_t font) {
  auto& t = display_tft();
  int16_t x = col_x(col);
  t.fillRect(x, y, kFieldW, h, kBgColor);
  t.setTextColor(fg, kBgColor);
  t.drawString(text, x, y, font);
}

// Per-port previous bar heights so render() only rewrites columns whose
// height changed since the last tick. 88 bytes per port; the bins[]
// scratch is shared across ports since they paint sequentially.
uint8_t  spark_prev_[3][kSparkBins]{};
bool     spark_prev_valid_[3] = {false, false, false};

void draw_sparkline(uint8_t col, const PortHistory& h, bool attached) {
  auto&   t  = display_tft();
  int16_t x0 = col_x(col);

  if (!attached || h.size() == 0) {
    t.fillRect(x0, kSparkY, kSparkBins, kSparkH, kBgColor);
    for (int16_t i = 0; i < kSparkBins; ++i) spark_prev_[col][i] = 0;
    spark_prev_valid_[col] = true;
    return;
  }

  static uint32_t bins[kSparkBins];
  h.power_downsample_mW(bins, kSparkBins, kSparkWindow);

  uint32_t peak = 0;
  for (size_t i = 0; i < kSparkBins; ++i) {
    if (bins[i] > peak) peak = bins[i];
  }
  if (peak == 0) {
    if (spark_prev_valid_[col]) {
      t.fillRect(x0, kSparkY, kSparkBins, kSparkH, kBgColor);
      for (int16_t i = 0; i < kSparkBins; ++i) spark_prev_[col][i] = 0;
    }
    spark_prev_valid_[col] = true;
    return;
  }

  bool first = !spark_prev_valid_[col];
  for (int16_t i = 0; i < kSparkBins; ++i) {
    uint32_t bar32 = (bins[i] * (uint32_t)kSparkH) / peak;
    if (bar32 == 0 && bins[i] > 0) bar32 = 1;
    uint8_t bar = (uint8_t)bar32;
    if (!first && bar == spark_prev_[col][i]) continue;

    t.drawFastVLine(x0 + i, kSparkY, kSparkH, kBgColor);
    if (bar > 0) {
      t.drawFastVLine(x0 + i, kSparkY + kSparkH - bar, bar, kSparkColor);
    }
    spark_prev_[col][i] = bar;
  }
  spark_prev_valid_[col] = true;
}

// Format milli-units (mV / mA / mW) as "X.YY<suffix>". Always integer
// math to avoid pulling in soft-float on M0+.
void format_centi(char* buf, size_t n, uint32_t milli, bool attached,
                  char suffix) {
  if (!attached) { snprintf(buf, n, "--"); return; }
  uint32_t cu = (milli + 5) / 10;
  snprintf(buf, n, "%lu.%02lu%c",
           (unsigned long)(cu / 100), (unsigned long)(cu % 100), suffix);
}

}  // namespace

void DisplayUi::begin() {
  draw_frame();
  for (auto& c : last_) c.valid = false;
  for (auto& v : spark_prev_valid_) v = false;
  last_total_mW_  = 0xFFFFFFFFu;
  last_elapsed_s_ = 0xFFFFFFFFu;
}

void DisplayUi::render(const PortSnapshot (&ports)[3], uint32_t total_mW,
                       uint32_t now_ms) {
  for (uint8_t i = 0; i < 3; ++i) {
    const PortSnapshot& p   = ports[i];
    uint32_t            w   = power_mW(p.live.v_mV, p.live.i_mA);
    Cache&              c   = last_[i];
    bool                init = !c.valid;
    bool                att  = init || c.attached != p.live.attached;

    char buf[16];
    if (att || c.v_mV != p.live.v_mV) {
      format_centi(buf, sizeof(buf), p.live.v_mV, p.live.attached, 'V');
      draw_value(i, kVoltageY, kBigFontH, kValueColor, buf, 4);
    }
    if (att || c.i_mA != p.live.i_mA) {
      format_centi(buf, sizeof(buf), p.live.i_mA, p.live.attached, 'A');
      draw_value(i, kCurrentY, kBigFontH, kValueColor, buf, 4);
    }
    if (att || c.w_mW != w) {
      format_centi(buf, sizeof(buf), w, p.live.attached, 'W');
      draw_value(i, kPowerY, kBigFontH, kValueColor, buf, 4);
    }
    if (att || c.proto != (uint8_t)p.live.proto) {
      draw_value(i, kProtoY, kSmallFontH, kProtoColor,
                 p.live.attached ? protocol_name(p.live.proto) : "--", 2);
    }
    if (init || c.phase != (uint8_t)p.phase) {
      draw_value(i, kPhaseY, kSmallFontH, phase_color(p.phase),
                 phase_name(p.phase), 2);
    }

    c.v_mV     = p.live.v_mV;
    c.i_mA     = p.live.i_mA;
    c.w_mW     = w;
    c.proto    = (uint8_t)p.live.proto;
    c.phase    = (uint8_t)p.phase;
    c.attached = p.live.attached;
    c.valid    = true;

    if (p.history) draw_sparkline(i, *p.history, p.live.attached);
  }

  uint32_t earliest_start = 0xFFFFFFFFu;
  for (uint8_t i = 0; i < 3; ++i) {
    if (ports[i].session.active &&
        ports[i].session.start_ms < earliest_start) {
      earliest_start = ports[i].session.start_ms;
    }
  }
  uint32_t elapsed_s = (earliest_start == 0xFFFFFFFFu)
                          ? 0u
                          : (now_ms - earliest_start) / 1000u;

  // Clear only the dirty subfield each tick — elapsed ticks every second
  // and would otherwise wipe the whole bar 1Hz.
  if (total_mW != last_total_mW_) {
    auto&    t = display_tft();
    char     buf[32];
    uint32_t cw = (total_mW + 5) / 10;
    snprintf(buf, sizeof(buf), "Total %lu.%02luW",
             (unsigned long)(cw / 100), (unsigned long)(cw % 100));
    t.fillRect(0, kSummaryY + 2, kElapsedX - 4, kScreenH - kSummaryY - 2,
               kBgColor);
    t.setTextColor(TFT_WHITE, kBgColor);
    t.drawString(buf, kSummaryPadX, kSummaryY + kSummaryPadY, 2);
    last_total_mW_ = total_mW;
  }
  if (elapsed_s != last_elapsed_s_) {
    auto& t = display_tft();
    char  buf[32];
    snprintf(buf, sizeof(buf), "t=%lus", (unsigned long)elapsed_s);
    t.fillRect(kElapsedX, kSummaryY + 2, kScreenW - kElapsedX,
               kScreenH - kSummaryY - 2, kBgColor);
    t.setTextColor(TFT_WHITE, kBgColor);
    t.drawString(buf, kElapsedX, kSummaryY + kSummaryPadY, 2);
    last_elapsed_s_ = elapsed_s;
  }
}
