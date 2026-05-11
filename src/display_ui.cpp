// 3-port View. Static frame painted once in begin(); render() repaints
// only the fields whose displayed (centi-precision) value changed.

#include "display_ui.h"

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <stdio.h>

#include "display.h"

namespace {

constexpr int16_t  kScreenW    = 320;
constexpr int16_t  kScreenH    = 240;
constexpr int16_t  kColW       = 106;
constexpr int16_t  kColSlack   = kScreenW - 3 * kColW;  // absorbed by col 2
constexpr int16_t  kColPadL    = 4;
constexpr int16_t  kColPadR    = 2;
constexpr int16_t  kBodyBottom = 220;
constexpr int16_t  kSummaryPadR = 6;
constexpr int16_t  kSummaryClearW = 140;
constexpr uint16_t kFrameColor  = 0x4208;  // dark grey
constexpr uint16_t kValueColor  = TFT_WHITE;
constexpr uint16_t kProtoColor  = TFT_CYAN;
constexpr uint16_t kSubColor    = 0xC618;  // light grey for V/A
constexpr uint16_t kBgColor     = TFT_BLACK;

constexpr int16_t kHeaderY  = 4;
constexpr int16_t kPowerY   = 32;
constexpr int16_t kVAY      = 76;
constexpr int16_t kProtoY   = 108;
constexpr int16_t kPhaseY   = 140;
constexpr int16_t kBigFontH = 26;
constexpr int16_t kRowH     = 16;

int16_t col_left(uint8_t i) { return i * kColW + kColPadL; }
int16_t col_right(uint8_t i) {
  int16_t edge = (i + 1) * kColW + (i == 2 ? kColSlack : 0);
  return edge - kColPadR;
}
int16_t col_width(uint8_t i) { return col_right(i) - col_left(i); }

// Keep this switch in lock-step with phase_name() in charge_analyzer.h —
// adding a Phase value requires touching both.
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

void draw_frame() {
  auto& t = display_tft();
  t.fillScreen(kBgColor);
  t.drawFastVLine(kColW,     0, kBodyBottom, kFrameColor);
  t.drawFastVLine(2 * kColW, 0, kBodyBottom, kFrameColor);
  for (uint8_t i = 0; i < 3; ++i) {
    char hdr[8];
    snprintf(hdr, sizeof(hdr), "Port%u", (unsigned)(i + 1));
    t.setTextColor(TFT_WHITE, kBgColor);
    t.drawString(hdr, col_left(i), kHeaderY, 2);
  }
}

void clear_row(uint8_t col, int16_t y, int16_t h) {
  display_tft().fillRect(col_left(col), y, col_width(col), h, kBgColor);
}

void draw_text(uint8_t col, int16_t y, int16_t h, uint16_t fg,
               const char* text, uint8_t font) {
  auto& t = display_tft();
  clear_row(col, y, h);
  t.setTextColor(fg, kBgColor);
  t.drawString(text, col_left(col), y, font);
}

void format_centi(char* buf, size_t n, uint32_t milli, bool attached,
                  char suffix) {
  if (!attached) { snprintf(buf, n, "--"); return; }
  uint32_t cu = (milli + 5) / 10;
  snprintf(buf, n, "%lu.%02lu%c",
           (unsigned long)(cu / 100), (unsigned long)(cu % 100), suffix);
}

void format_va(char* buf, size_t n, uint16_t v_mV, uint16_t i_mA,
               bool attached) {
  if (!attached) { snprintf(buf, n, "--"); return; }
  char v[12], a[12];
  format_centi(v, sizeof(v), v_mV, true, 'V');
  format_centi(a, sizeof(a), i_mA, true, 'A');
  snprintf(buf, n, "%s  %s", v, a);
}

void format_elapsed(char* buf, size_t n, uint32_t s) {
  uint32_t h  = s / 3600u;
  uint32_t m  = (s / 60u) % 60u;
  uint32_t ss = s % 60u;
  snprintf(buf, n, "%lu:%02lu:%02lu",
           (unsigned long)h, (unsigned long)m, (unsigned long)ss);
}

inline uint32_t centi(uint32_t milli) { return (milli + 5) / 10; }

}  // namespace

void DisplayUi::begin() {
  draw_frame();
  for (auto& c : last_) c.valid = false;
  last_total_mW_ = 0xFFFFFFFFu;
}

void DisplayUi::render(const PortSnapshot (&ports)[3], uint32_t total_mW,
                       uint32_t now_ms) {
  for (uint8_t i = 0; i < 3; ++i) {
    const PortSnapshot& p    = ports[i];
    // Pre-quantize to the precision actually drawn so mV/mA-level jitter
    // does not invalidate the cache when the displayed text is unchanged.
    uint32_t            cw   = centi(power_mW(p.live.v_mV, p.live.i_mA));
    uint32_t            cv   = centi(p.live.v_mV);
    uint32_t            ci   = centi(p.live.i_mA);
    uint32_t            es   = session_elapsed_s(p.session, now_ms);
    Cache&              c    = last_[i];
    bool                init = !c.valid;
    bool                att  = init || c.attached != p.live.attached;

    if (att || c.w_mW != cw) {
      char buf[24];
      format_centi(buf, sizeof(buf),
                   power_mW(p.live.v_mV, p.live.i_mA),
                   p.live.attached, 'W');
      draw_text(i, kPowerY, kBigFontH, kValueColor, buf, 4);
    }
    if (att || c.v_mV != cv || c.i_mA != ci) {
      char buf[24];
      format_va(buf, sizeof(buf), p.live.v_mV, p.live.i_mA, p.live.attached);
      draw_text(i, kVAY, kRowH, kSubColor, buf, 2);
    }
    if (att || c.proto != (uint8_t)p.live.proto || c.elapsed_s != es) {
      char line[24];
      if (p.live.attached) {
        char tbuf[16];
        format_elapsed(tbuf, sizeof(tbuf), es);
        snprintf(line, sizeof(line), "%s  %s",
                 protocol_name(p.live.proto), tbuf);
      } else {
        snprintf(line, sizeof(line), "--");
      }
      draw_text(i, kProtoY, kRowH, kProtoColor, line, 2);
    }
    if (init || c.phase != (uint8_t)p.phase) {
      draw_text(i, kPhaseY, kRowH, phase_color(p.phase),
                phase_name(p.phase), 2);
    }

    c.v_mV      = (uint16_t)cv;
    c.i_mA      = (uint16_t)ci;
    c.w_mW      = cw;
    c.elapsed_s = es;
    c.proto     = (uint8_t)p.live.proto;
    c.phase     = (uint8_t)p.phase;
    c.attached  = p.live.attached;
    c.valid     = true;
  }

  uint32_t ct = centi(total_mW);
  if (ct != last_total_mW_) {
    auto& t = display_tft();
    char  buf[32];
    snprintf(buf, sizeof(buf), "Total %lu.%02luW",
             (unsigned long)(ct / 100), (unsigned long)(ct % 100));
    t.fillRect(kScreenW - kSummaryClearW, kBodyBottom + 2,
               kSummaryClearW, kScreenH - kBodyBottom - 2, kBgColor);
    t.setTextColor(TFT_WHITE, kBgColor);
    t.drawRightString(buf, kScreenW - kSummaryPadR, kBodyBottom + 4, 2);
    last_total_mW_ = ct;
  }
}
