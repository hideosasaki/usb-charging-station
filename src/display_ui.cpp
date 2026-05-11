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
constexpr int16_t  kColSlack   = kScreenW - 3 * kColW;
constexpr int16_t  kColPadL    = 4;
constexpr int16_t  kColPadR    = 2;
constexpr int16_t  kBodyBottom = 220;
constexpr int16_t  kSummaryPadR   = 6;
constexpr int16_t  kSummaryClearW = 140;
// Material Dark on a black background. Greys/accents only — the surface
// stays full black so the OLED-style contrast carries through.
constexpr uint16_t kFrameColor = 0x39C7;  // outline #3A3A3A
constexpr uint16_t kValueColor = 0xE71C;  // on-surface #E0E0E0
constexpr uint16_t kSubColor   = 0xA514;  // on-surface-variant #A0A0A0
constexpr uint16_t kAccentOk   = 0x8630;  // green 300 #81C784
constexpr uint16_t kBgColor    = TFT_BLACK;

constexpr int16_t kHeaderY    = 4;
constexpr int16_t kPillY      = 30;
constexpr int16_t kPillH      = 18;
constexpr int16_t kPowerY     = 64;
constexpr int16_t kPowerH     = 26;
constexpr int16_t kVAY        = 96;
constexpr int16_t kClockY     = 118;
constexpr int16_t kClockRowH  = 16;
constexpr int16_t kClockR     = 6;
constexpr int16_t kProgressY  = 146;
constexpr int16_t kProgressH  = 10;
constexpr int16_t kEtaY       = 166;
constexpr int16_t kEnergyY    = 188;
constexpr int16_t kRowH       = 16;
constexpr uint16_t kProgressFill   = kAccentOk;
constexpr uint16_t kProgressBorder = kFrameColor;

int16_t col_left(uint8_t i) { return i * kColW + kColPadL; }
int16_t col_right(uint8_t i) {
  int16_t edge = (i + 1) * kColW + (i == 2 ? kColSlack : 0);
  return edge - kColPadR;
}
int16_t col_width(uint8_t i) { return col_right(i) - col_left(i); }

uint16_t proto_color(Protocol p) {
  switch (p) {
    case Protocol::Std5V:   return 0x4DB5;  // teal 300 #4DB6AC
    case Protocol::Qc20:    return 0xBB59;  // purple 300 #BA68C8
    case Protocol::Qc30:    return 0x93B9;  // deep purple 300 #9575CD
    case Protocol::Pd20:    return 0x4E1E;  // light blue 300 #4FC3F7
    case Protocol::Pd30:    return 0x65BE;  // blue 300 #64B5F6
    case Protocol::Pd31Pps: return 0x1FFF;  // cyan A200 #18FFFF
    case Protocol::None:    return kBgColor;
  }
  return kBgColor;
}

void draw_frame() {
  auto& t = display_tft();
  t.fillScreen(kBgColor);
  t.drawFastVLine(kColW,     0, kBodyBottom, kFrameColor);
  t.drawFastVLine(2 * kColW, 0, kBodyBottom, kFrameColor);
  for (uint8_t i = 0; i < 3; ++i) {
    char hdr[8];
    snprintf(hdr, sizeof(hdr), "Port %u", (unsigned)i);
    t.setTextColor(kValueColor, kBgColor);
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

void draw_pill(uint8_t col, Protocol proto, bool attached) {
  auto&   t = display_tft();
  int16_t x = col_left(col);
  int16_t w = col_width(col);
  t.fillRect(x, kPillY, w, kPillH, kBgColor);
  if (!attached || proto == Protocol::None) return;
  const char* name = protocol_name(proto);
  int16_t     tw   = t.textWidth(name, 2);
  int16_t     pw   = tw + 12;
  if (pw > w) pw = w;
  uint16_t bg = proto_color(proto);
  t.fillRoundRect(x, kPillY, pw, kPillH, 4, bg);
  t.setTextColor(TFT_WHITE, bg);
  t.drawString(name, x + 6, kPillY + 1, 2);
}

void draw_clock_icon(int16_t cx, int16_t cy) {
  auto& t = display_tft();
  t.drawCircle(cx, cy, kClockR, kSubColor);
  t.drawLine(cx, cy, cx, cy - (kClockR - 2), kSubColor);
  t.drawLine(cx, cy, cx + (kClockR - 1), cy, kSubColor);
}

// Font2 ":" packs the two dots so tightly the time is hard to scan, so
// the separators are drawn by hand with a wider vertical gap.
void draw_hms(int16_t x, int16_t y, uint32_t total_s, uint16_t fg) {
  auto& t = display_tft();
  t.setTextColor(fg, kBgColor);
  uint32_t h  = total_s / 3600u;
  uint32_t m  = (total_s / 60u) % 60u;
  uint32_t s  = total_s % 60u;
  char     h_buf[8];
  snprintf(h_buf, sizeof(h_buf), "%lu", (unsigned long)h);
  t.drawString(h_buf, x, y, 2);
  x += t.textWidth(h_buf, 2);

  auto draw_colon = [&](int16_t cx) {
    t.fillRect(cx, y + 5,  2, 2, fg);
    t.fillRect(cx, y + 10, 2, 2, fg);
  };

  draw_colon(x + 2); x += 6;
  char mm[4];
  snprintf(mm, sizeof(mm), "%02lu", (unsigned long)m);
  t.drawString(mm, x, y, 2);
  x += t.textWidth(mm, 2);

  draw_colon(x + 2); x += 6;
  char ss[4];
  snprintf(ss, sizeof(ss), "%02lu", (unsigned long)s);
  t.drawString(ss, x, y, 2);
}

// Repaint policy: the clock face is static, so it is only drawn on the
// attach edge. The H:MM:SS half clears and rewrites every tick.
void draw_clock_row(uint8_t col, uint32_t elapsed_s, bool attached,
                    bool force_full_redraw) {
  int16_t x = col_left(col);
  if (force_full_redraw) {
    clear_row(col, kClockY, kClockRowH);
    if (!attached) return;
    draw_clock_icon(x + kClockR, kClockY + kClockRowH / 2);
  } else if (!attached) {
    return;
  }
  int16_t hms_x = x + 2 * kClockR + 4;
  auto&   t     = display_tft();
  t.fillRect(hms_x, kClockY, col_right(col) - hms_x, kClockRowH, kBgColor);
  draw_hms(hms_x, kClockY, elapsed_s, kValueColor);
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

void draw_power(uint8_t col, uint32_t w_mW, bool attached) {
  auto&   t = display_tft();
  int16_t x = col_left(col);
  t.fillRect(x, kPowerY, col_width(col), kPowerH, kBgColor);
  if (!attached) {
    t.setTextColor(kSubColor, kBgColor);
    t.drawString("--", x, kPowerY, 4);
    return;
  }
  char buf[16];
  format_centi(buf, sizeof(buf), w_mW, true, 'W');
  t.setTextColor(kValueColor, kBgColor);
  t.drawString(buf, x, kPowerY, 4);
}

void draw_eta_row(uint8_t col, Phase phase, EtaSeconds eta, bool attached) {
  if (!attached) { clear_row(col, kEtaY, kRowH); return; }
  if (phase == Phase::Idle) {
    draw_text(col, kEtaY, kRowH, kSubColor, "Standby", 2);
    return;
  }
  if (phase == Phase::Done) {
    draw_text(col, kEtaY, kRowH, kAccentOk, "Done", 2);
    return;
  }
  char        body_buf[16];
  const char* body;
  if (!eta.valid) {
    body = "?";
  } else if (eta.seconds < 60u) {
    body = "<1m";
  } else {
    uint32_t h = eta.seconds / 3600u;
    uint32_t m = (eta.seconds / 60u) % 60u;
    if (h > 0) {
      snprintf(body_buf, sizeof(body_buf), "%luh %02lum",
               (unsigned long)h, (unsigned long)m);
    } else {
      snprintf(body_buf, sizeof(body_buf), "%lum", (unsigned long)m);
    }
    body = body_buf;
  }
  char buf[24];
  snprintf(buf, sizeof(buf), "ETA %s", body);
  draw_text(col, kEtaY, kRowH, kValueColor, buf, 2);
}

inline uint32_t centi(uint32_t milli) { return (milli + 5) / 10; }

void draw_progress_bar(uint8_t col, uint8_t pct, bool valid, bool attached) {
  auto&   t = display_tft();
  int16_t x = col_left(col);
  int16_t w = col_width(col);
  clear_row(col, kProgressY, kProgressH);
  if (!attached) return;
  t.drawRect(x, kProgressY, w, kProgressH, kProgressBorder);
  if (!valid) return;
  int16_t fill = (int16_t)((uint32_t)(w - 2) * pct / 100u);
  if (fill > 0) {
    t.fillRect(x + 1, kProgressY + 1, fill, kProgressH - 2, kProgressFill);
  }
}

void draw_energy_row(uint8_t col, uint32_t cwh, bool attached) {
  clear_row(col, kEnergyY, kRowH);
  if (!attached) return;
  char buf[24];
  snprintf(buf, sizeof(buf), "%lu.%02luWh",
           (unsigned long)(cwh / 100), (unsigned long)(cwh % 100));
  auto& t = display_tft();
  t.setTextColor(kValueColor, kBgColor);
  t.drawString(buf, col_left(col), kEnergyY, 2);
}

// ETA needs the full averaging window to make a meaningful slope, so
// fall back to zero if PortHistory has fewer samples than requested
// (PortHistory::avg_i_mA would otherwise average over what it has).
uint16_t avg_i_full_window(const PortHistory* h, size_t seconds) {
  if (!h || h->size() < seconds) return 0;
  return h->avg_total_i_mA(seconds);
}

EtaSeconds compute_eta(const PortSnapshot& p) {
  if (!p.live.attached) return {0, false};
  uint16_t recent = avg_i_full_window(p.history, kEtaRecentWindow_s);
  uint16_t old    = avg_i_full_window(p.history, kEtaOldWindow_s);
  return eta_seconds(reading_total_i_mA(p.live), recent, old, p.phase);
}

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
    uint16_t live_i_mA = reading_total_i_mA(p.live);
    uint32_t w_mW = power_mW(p.live.v_mV, live_i_mA);
    uint32_t cw   = centi(w_mW);
    uint32_t cv   = centi(p.live.v_mV);
    uint32_t ci   = centi(live_i_mA);
    uint32_t es   = session_elapsed_s(p.session, now_ms);
    uint32_t ce   = centi(p.session.energy_mWh);
    Cache&   c    = last_[i];
    bool     init = !c.valid;
    bool     att  = init || c.attached != p.live.attached;

    if (att || c.proto != (uint8_t)p.live.proto) {
      draw_pill(i, p.live.proto, p.live.attached);
    }
    if (att || c.w_mW != cw) {
      draw_power(i, w_mW, p.live.attached);
    }
    if (att || c.v_mV != cv || c.i_mA != ci) {
      char buf[24];
      format_va(buf, sizeof(buf), p.live.v_mV, live_i_mA, p.live.attached);
      draw_text(i, kVAY, kRowH, kSubColor, buf, 2);
    }
    if (att || c.elapsed_s != es) {
      draw_clock_row(i, es, p.live.attached, att);
    }

    ChargeProgress prog = p.live.attached
        ? charge_progress(p.session.peak_i_mA, live_i_mA, p.phase)
        : ChargeProgress{0, false};
    EtaSeconds     eta = compute_eta(p);
    bool prog_changed = c.progress_pct != prog.pct ||
                        c.progress_valid != prog.valid;
    // Quantize to minutes since the row only shows "Xh YYm" / "<1m".
    uint32_t eta_minute_bucket =
        (eta.seconds < 60u) ? 0u : (eta.seconds / 60u);
    bool eta_changed = c.eta_s != eta_minute_bucket ||
                       c.eta_valid != eta.valid ||
                       c.phase != (uint8_t)p.phase;
    if (att || prog_changed) {
      draw_progress_bar(i, prog.pct, prog.valid, p.live.attached);
    }
    if (att || eta_changed) {
      draw_eta_row(i, p.phase, eta, p.live.attached);
    }
    if (att || c.energy_cWh != ce) {
      draw_energy_row(i, ce, p.live.attached);
    }

    c.v_mV           = (uint16_t)cv;
    c.i_mA           = (uint16_t)ci;
    c.w_mW           = cw;
    c.elapsed_s      = es;
    c.energy_cWh     = ce;
    c.eta_s          = eta_minute_bucket;
    c.proto          = (uint8_t)p.live.proto;
    c.phase          = (uint8_t)p.phase;
    c.progress_pct   = prog.pct;
    c.progress_valid = prog.valid;
    c.eta_valid      = eta.valid;
    c.attached       = p.live.attached;
    c.valid          = true;
  }

  uint32_t ct = centi(total_mW);
  if (ct != last_total_mW_) {
    auto& t = display_tft();
    char  buf[32];
    snprintf(buf, sizeof(buf), "Total %lu.%02luW",
             (unsigned long)(ct / 100), (unsigned long)(ct % 100));
    t.fillRect(kScreenW - kSummaryClearW, kBodyBottom + 2,
               kSummaryClearW, kScreenH - kBodyBottom - 2, kBgColor);
    t.setTextColor(kValueColor, kBgColor);
    t.drawRightString(buf, kScreenW - kSummaryPadR, kBodyBottom + 4, 2);
    last_total_mW_ = ct;
  }
}
