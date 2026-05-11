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

// One off-screen buffer wide and tall enough to satisfy every caller of
// paint_rect(); reusing it avoids per-frame allocations and the static
// dimensions let the constant fit in flash.
constexpr int16_t kMaxRowW =
    (kSummaryClearW > kColW + kColSlack - kColPadL - kColPadR)
    ? kSummaryClearW
    : kColW + kColSlack - kColPadL - kColPadR;
constexpr int16_t kMaxRowH = kPowerH;

// Constructed lazily in DisplayUi::begin() so the TFT_eSPI instance it
// binds to is guaranteed to exist — a global TFT_eSprite would race the
// static init order against the TFT_eSPI in display.cpp.
TFT_eSprite* g_row_sprite = nullptr;

bool row_sprite_ready() {
  return g_row_sprite && g_row_sprite->created();
}

// Dual-rail tile coordinates. Used only when rail_mask == 0x3.
// Both rails render the same four rows; the divider sits between them.
struct RailRowYs {
  int16_t pill;
  int16_t power;
  int16_t progress;
  int16_t eta;
};
// The C pill sits at the same Y as the single-rail pill so the gap
// below the "Port N" header is consistent across layouts. The two
// rails are separated by 16 px of empty space — the rail pill labels
// ("Type-C ..." / "Type-A ...") are enough to tell them apart without
// a drawn border.
constexpr RailRowYs kDualCRows = {30,  54,  84,  98};
constexpr RailRowYs kDualARows = {130, 154, 184, 198};

const char* rail_short_name(Rail r) {
  return r == Rail::UsbC ? "Type-C" : "Type-A";
}

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

// Wipe everything below the "Port N" header. Used on a layout switch
// (single <-> dual) so stale rows from the previous layout do not
// bleed through the next frame.
void clear_tile_body(uint8_t col) {
  int16_t y = kHeaderY + kRowH;
  display_tft().fillRect(col_left(col), y, col_width(col),
                         kBodyBottom - y, kBgColor);
}

// Repaint a rectangle through the shared off-screen sprite. The draw
// functor receives a TFT-like surface plus the origin to draw at:
// (0, 0) when rendering into the sprite, (x, y) for the direct
// fallback path that runs only when the sprite could not be allocated.
// The sprite path is the whole point of this helper — it hides the
// "fillRect(bg) → drawString" two-step that otherwise lets the eraser
// flash through to the panel.
template <typename F>
void paint_rect(int16_t x, int16_t y, int16_t w, int16_t h, F draw_fn) {
  if (row_sprite_ready() && w <= kMaxRowW && h <= kMaxRowH) {
    // Clear only the (w,h) sub-region rather than the whole sprite —
    // the sprite is sized for the widest caller, so a full fill would
    // touch up to 2× the pixels actually pushed.
    g_row_sprite->fillRect(0, 0, w, h, kBgColor);
    draw_fn(*g_row_sprite, (int16_t)0, (int16_t)0);
    g_row_sprite->pushSprite(x, y, /*sx=*/0, /*sy=*/0, w, h);
  } else {
    auto& t = display_tft();
    t.fillRect(x, y, w, h, kBgColor);
    draw_fn(t, x, y);
  }
}

template <typename F>
void paint_row(uint8_t col, int16_t y, int16_t w, int16_t h, F draw_fn) {
  paint_rect(col_left(col), y, w, h, draw_fn);
}

void draw_text(uint8_t col, int16_t y, int16_t h, uint16_t fg,
               const char* text, uint8_t font) {
  paint_row(col, y, col_width(col), h,
            [&](TFT_eSPI& t, int16_t ox, int16_t oy) {
              t.setTextColor(fg, kBgColor);
              t.drawString(text, ox, oy, font);
            });
}

void draw_pill_at(uint8_t col, int16_t y, const char* name, uint16_t bg) {
  int16_t w = col_width(col);
  paint_row(col, y, w, kPillH,
            [&](TFT_eSPI& t, int16_t ox, int16_t oy) {
              if (!name || !*name) return;
              int16_t tw = t.textWidth(name, 2);
              int16_t pw = tw + 12;
              if (pw > w) pw = w;
              t.fillRoundRect(ox, oy, pw, kPillH, 4, bg);
              t.setTextColor(TFT_WHITE, bg);
              t.drawString(name, ox + 6, oy + 1, 2);
            });
}

void draw_pill(uint8_t col, Protocol proto, bool attached) {
  if (!attached || proto == Protocol::None) {
    draw_pill_at(col, kPillY, nullptr, kBgColor);
    return;
  }
  draw_pill_at(col, kPillY, protocol_name(proto), proto_color(proto));
}

// Label for a rail-tagged pill: "Type-C PROTO" / "Type-A 5V". The rail
// prefix tells the user which connector this row represents when both
// rails are active.
void draw_rail_pill(uint8_t col, int16_t y, Rail rail, Protocol proto,
                    bool attached) {
  if (!attached) {
    draw_pill_at(col, y, nullptr, kBgColor);
    return;
  }
  char label[16];
  snprintf(label, sizeof(label), "%s %s",
           rail_short_name(rail), protocol_name(proto));
  draw_pill_at(col, y, label, proto_color(proto));
}

void draw_clock_icon(TFT_eSPI& t, int16_t cx, int16_t cy) {
  t.drawCircle(cx, cy, kClockR, kSubColor);
  t.drawLine(cx, cy, cx, cy - (kClockR - 2), kSubColor);
  t.drawLine(cx, cy, cx + (kClockR - 1), cy, kSubColor);
}

// Font2 ":" packs the two dots so tightly the time is hard to scan, so
// the separators are drawn by hand with a wider vertical gap.
void draw_hms(TFT_eSPI& t, int16_t x, int16_t y, uint32_t total_s,
              uint16_t fg) {
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

// Atomic per-second repaint: icon and H:MM:SS land in one sprite push,
// so the row never shows a half-erased intermediate.
void draw_clock_row(uint8_t col, uint32_t elapsed_s, bool attached) {
  paint_row(col, kClockY, col_width(col), kClockRowH,
            [&](TFT_eSPI& t, int16_t ox, int16_t oy) {
              if (!attached) return;
              draw_clock_icon(t, ox + kClockR, oy + kClockRowH / 2);
              draw_hms(t, ox + 2 * kClockR + 4, oy, elapsed_s, kValueColor);
            });
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

void draw_power_at(uint8_t col, int16_t y, uint32_t w_mW, bool attached) {
  paint_row(col, y, col_width(col), kPowerH,
            [&](TFT_eSPI& t, int16_t ox, int16_t oy) {
              if (!attached) {
                t.setTextColor(kSubColor, kBgColor);
                t.drawString("--", ox, oy, 4);
                return;
              }
              char buf[16];
              format_centi(buf, sizeof(buf), w_mW, true, 'W');
              t.setTextColor(kValueColor, kBgColor);
              t.drawString(buf, ox, oy, 4);
            });
}

void draw_power(uint8_t col, uint32_t w_mW, bool attached) {
  draw_power_at(col, kPowerY, w_mW, attached);
}

void draw_eta_row_at(uint8_t col, int16_t y, Phase phase, EtaSeconds eta,
                     bool attached) {
  if (!attached) { draw_text(col, y, kRowH, kSubColor, "", 2); return; }
  if (phase == Phase::Idle) {
    draw_text(col, y, kRowH, kSubColor, "Standby", 2);
    return;
  }
  if (phase == Phase::Done) {
    draw_text(col, y, kRowH, kAccentOk, "Done", 2);
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
  draw_text(col, y, kRowH, kValueColor, buf, 2);
}

void draw_eta_row(uint8_t col, Phase phase, EtaSeconds eta, bool attached) {
  draw_eta_row_at(col, kEtaY, phase, eta, attached);
}

inline uint32_t centi(uint32_t milli) { return (milli + 5) / 10; }

void draw_progress_bar_at(uint8_t col, int16_t y, uint8_t pct, bool valid,
                          bool attached) {
  int16_t w = col_width(col);
  paint_row(col, y, w, kProgressH,
            [&](TFT_eSPI& t, int16_t ox, int16_t oy) {
              if (!attached) return;
              t.drawRect(ox, oy, w, kProgressH, kProgressBorder);
              if (!valid) return;
              int16_t fill = (int16_t)((uint32_t)(w - 2) * pct / 100u);
              if (fill > 0) {
                t.fillRect(ox + 1, oy + 1, fill,
                           kProgressH - 2, kProgressFill);
              }
            });
}

void draw_progress_bar(uint8_t col, uint8_t pct, bool valid, bool attached) {
  draw_progress_bar_at(col, kProgressY, pct, valid, attached);
}

void draw_energy_row(uint8_t col, uint32_t cwh, bool attached) {
  paint_row(col, kEnergyY, col_width(col), kRowH,
            [&](TFT_eSPI& t, int16_t ox, int16_t oy) {
              if (!attached) return;
              char buf[24];
              snprintf(buf, sizeof(buf), "%lu.%02luWh",
                       (unsigned long)(cwh / 100),
                       (unsigned long)(cwh % 100));
              t.setTextColor(kValueColor, kBgColor);
              t.drawString(buf, ox, oy, 2);
            });
}

// ETA needs the full averaging window to make a meaningful slope, so
// fall back to zero if PortHistory has fewer samples than requested
// (PortHistory::avg_i_mA would otherwise average over what it has).
uint16_t avg_i_full_window(const PortHistory* h, size_t seconds, Rail rail) {
  if (!h || h->size() < seconds) return 0;
  return h->avg_i_mA(seconds, rail);
}

EtaSeconds compute_eta_for(const PortSnapshot& p, Rail rail) {
  if (!p.live.has(rail)) return {0, false};
  uint16_t recent = avg_i_full_window(p.history, kEtaRecentWindow_s, rail);
  uint16_t old    = avg_i_full_window(p.history, kEtaOldWindow_s, rail);
  return eta_seconds(p.live.i_mA(rail), recent, old,
                     p.phase[(uint8_t)rail]);
}

EtaSeconds compute_eta(const PortSnapshot& p) {
  return compute_eta_for(p, p.live.active_rail());
}

// Render one rail's stack (pill / power / progress / ETA) at the given
// row coordinates. Used by the dual-rail tile layout.
void draw_rail_stack(uint8_t col, Rail rail, const PortSnapshot& p,
                     const RailRowYs& rows) {
  bool     att  = p.live.has(rail);
  uint16_t i_mA = p.live.i_mA(rail);
  Phase    ph   = p.phase[(uint8_t)rail];
  const SessionStats& sess = p.session[(uint8_t)rail];

  draw_rail_pill(col, rows.pill, rail, p.live.proto, att);
  draw_power_at(col, rows.power, power_mW(p.live.v_mV, i_mA), att);

  ChargeProgress prog = att
      ? charge_progress(sess.peak_i_mA, i_mA, ph)
      : ChargeProgress{0, false};
  draw_progress_bar_at(col, rows.progress, prog.pct, prog.valid, att);

  EtaSeconds eta = compute_eta_for(p, rail);
  draw_eta_row_at(col, rows.eta, ph, eta, att);
}

void draw_dual_tile(uint8_t col, const PortSnapshot& p) {
  draw_rail_stack(col, Rail::UsbC, p, kDualCRows);
  draw_rail_stack(col, Rail::UsbA, p, kDualARows);
}

}  // namespace

void DisplayUi::begin() {
  if (!g_row_sprite) {
    g_row_sprite = new TFT_eSprite(&display_tft());
    g_row_sprite->setColorDepth(16);
    g_row_sprite->createSprite(kMaxRowW, kMaxRowH);
  }
  draw_frame();
  for (auto& c : last_) c.valid = false;
  last_total_mW_ = 0xFFFFFFFFu;
}

void DisplayUi::render(const PortSnapshot (&ports)[3], uint32_t total_mW,
                       uint32_t now_ms) {
  for (uint8_t i = 0; i < 3; ++i) {
    const PortSnapshot& p = ports[i];
    bool     live_att  = p.live.attached();
    uint16_t live_i_mA = p.live.total_i_mA();
    Cache&   c         = last_[i];

    // Layout switches (single <-> dual) repaint the whole tile so stale
    // rows from the other layout do not bleed through.
    if (c.valid && c.rail_mask != p.live.rail_mask) {
      clear_tile_body(i);
      c.valid = false;
    }

    if (p.live.rail_mask == 0x3) {
      draw_dual_tile(i, p);
      // The single-rail value fields (w_mW, proto, phase, ...) do not
      // describe dual mode; zero the cache so a future re-entry to
      // single mode doesn't compare against stale values.
      c = Cache{};
      c.rail_mask = p.live.rail_mask;
      c.attached  = true;
      c.valid     = true;
      continue;
    }

    const SessionStats& sess  = p.active_session();
    Phase               phase = p.active_phase();
    uint32_t w_mW = power_mW(p.live.v_mV, live_i_mA);
    uint32_t cw   = centi(w_mW);
    uint32_t cv   = centi(p.live.v_mV);
    uint32_t ci   = centi(live_i_mA);
    uint32_t es   = session_elapsed_s(sess, now_ms);
    uint32_t ce   = centi(sess.energy_mWh);
    bool     init = !c.valid;
    bool     att  = init || c.attached != live_att;

    if (att || c.proto != (uint8_t)p.live.proto) {
      draw_pill(i, p.live.proto, live_att);
    }
    if (att || c.w_mW != cw) {
      draw_power(i, w_mW, live_att);
    }
    if (att || c.v_mV != cv || c.i_mA != ci) {
      char buf[24];
      format_va(buf, sizeof(buf), p.live.v_mV, live_i_mA, live_att);
      draw_text(i, kVAY, kRowH, kSubColor, buf, 2);
    }
    if (att || c.elapsed_s != es) {
      draw_clock_row(i, es, live_att);
    }

    ChargeProgress prog = live_att
        ? charge_progress(sess.peak_i_mA, live_i_mA, phase)
        : ChargeProgress{0, false};
    EtaSeconds     eta = compute_eta(p);
    bool prog_changed = c.progress_pct != prog.pct ||
                        c.progress_valid != prog.valid;
    // Quantize to minutes since the row only shows "Xh YYm" / "<1m".
    uint32_t eta_minute_bucket =
        (eta.seconds < 60u) ? 0u : (eta.seconds / 60u);
    bool eta_changed = c.eta_s != eta_minute_bucket ||
                       c.eta_valid != eta.valid ||
                       c.phase != (uint8_t)phase;
    if (att || prog_changed) {
      draw_progress_bar(i, prog.pct, prog.valid, live_att);
    }
    if (att || eta_changed) {
      draw_eta_row(i, phase, eta, live_att);
    }
    if (att || c.energy_cWh != ce) {
      draw_energy_row(i, ce, live_att);
    }

    c.v_mV           = (uint16_t)cv;
    c.i_mA           = (uint16_t)ci;
    c.w_mW           = cw;
    c.elapsed_s      = es;
    c.energy_cWh     = ce;
    c.eta_s          = eta_minute_bucket;
    c.proto          = (uint8_t)p.live.proto;
    c.phase          = (uint8_t)phase;
    c.progress_pct   = prog.pct;
    c.rail_mask      = p.live.rail_mask;
    c.progress_valid = prog.valid;
    c.eta_valid      = eta.valid;
    c.attached       = live_att;
    c.valid          = true;
  }

  uint32_t ct = centi(total_mW);
  if (ct != last_total_mW_) {
    constexpr int16_t kSummaryX = kScreenW - kSummaryClearW;
    constexpr int16_t kSummaryY = kBodyBottom + 2;
    constexpr int16_t kSummaryH = kScreenH - kBodyBottom - 2;
    char buf[32];
    snprintf(buf, sizeof(buf), "Total %lu.%02luW",
             (unsigned long)(ct / 100), (unsigned long)(ct % 100));
    paint_rect(kSummaryX, kSummaryY, kSummaryClearW, kSummaryH,
               [&](TFT_eSPI& t, int16_t ox, int16_t oy) {
                 t.setTextColor(kValueColor, kBgColor);
                 t.drawRightString(buf,
                                   ox + kSummaryClearW - kSummaryPadR,
                                   oy + 2, 2);
               });
    last_total_mW_ = ct;
  }
}
