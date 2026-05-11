// Serial command parser. Splits one line on whitespace, dispatches to
// the matching MockPortReader action, and writes a short status line
// when requested. The dispatch path is Arduino-free so native tests
// reach it directly; serial_cmd_poll() is the only Arduino-dependent
// glue and is compiled out under UNIT_TEST.

#include "serial_cmd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mock_port_reader.h"

namespace {

MockPortReader** g_readers = nullptr;

const char* kTokenSeps = " \t\r\n";

MockPortReader* resolve_port(char*& saveptr, MockPortReader* readers[3],
                             CmdResult& err) {
  char* tok = strtok_r(nullptr, kTokenSeps, &saveptr);
  if (!tok) { err = CmdResult::BadArgs; return nullptr; }
  char* end = nullptr;
  long v = strtol(tok, &end, 10);
  if (end == tok || *end != '\0' || v < 0) {
    err = CmdResult::BadArgs;
    return nullptr;
  }
  if (v >= 3 || readers[v] == nullptr) {
    err = CmdResult::OutOfRange;
    return nullptr;
  }
  return readers[v];
}

bool parse_u16(const char* tok, uint16_t& out) {
  if (!tok) return false;
  char* end = nullptr;
  long v = strtol(tok, &end, 10);
  if (end == tok || *end != '\0') return false;
  if (v < 0 || v > 65535) return false;
  out = (uint16_t)v;
  return true;
}

bool parse_protocol(const char* tok, Protocol& out) {
  if (!tok) return false;
  // Accept canonical printed forms ("QC2.0", "PD3.0", ...) — so `status`
  // output round-trips back as input — plus dotless aliases for typing.
  if (!strcasecmp(tok, "--") ||
      !strcasecmp(tok, "NONE"))         { out = Protocol::None;    return true; }
  if (!strcasecmp(tok, "5V") ||
      !strcasecmp(tok, "STD5V"))        { out = Protocol::Std5V;   return true; }
  if (!strcasecmp(tok, "QC2.0") ||
      !strcasecmp(tok, "QC20"))         { out = Protocol::Qc20;    return true; }
  if (!strcasecmp(tok, "QC3.0") ||
      !strcasecmp(tok, "QC30"))         { out = Protocol::Qc30;    return true; }
  if (!strcasecmp(tok, "PD2.0") ||
      !strcasecmp(tok, "PD20"))         { out = Protocol::Pd20;    return true; }
  if (!strcasecmp(tok, "PD3.0") ||
      !strcasecmp(tok, "PD30"))         { out = Protocol::Pd30;    return true; }
  if (!strcasecmp(tok, "PPS") ||
      !strcasecmp(tok, "PD31PPS"))      { out = Protocol::Pd31Pps; return true; }
  return false;
}

bool parse_scenario_id(const char* tok, ScenarioId& out) {
  if (!tok || tok[0] == '\0' || tok[1] != '\0') return false;
  switch (tok[0]) {
    case 'A': case 'a': out = ScenarioId::A_Pd30Phone;   return true;
    case 'B': case 'b': out = ScenarioId::B_Std5VSteady; return true;
    case 'C': case 'c': out = ScenarioId::C_IdleBurst;   return true;
  }
  return false;
}

CmdResult cmd_port(char* saveptr, MockPortReader* readers[3]) {
  CmdResult err;
  MockPortReader* port = resolve_port(saveptr, readers, err);
  if (!port) return err;

  char* arg1 = strtok_r(nullptr, kTokenSeps, &saveptr);
  if (!arg1) return CmdResult::BadArgs;
  if (!strcasecmp(arg1, "detach")) { port->force_detach(); return CmdResult::Ok; }
  if (!strcasecmp(arg1, "attach")) { port->force_attach(); return CmdResult::Ok; }
  if (!strcasecmp(arg1, "auto"))   { port->resume_auto();  return CmdResult::Ok; }

  uint16_t v_mV;
  if (!parse_u16(arg1, v_mV)) return CmdResult::BadArgs;
  char* i_tok = strtok_r(nullptr, kTokenSeps, &saveptr);
  char* p_tok = strtok_r(nullptr, kTokenSeps, &saveptr);
  if (!i_tok || !p_tok) return CmdResult::BadArgs;
  uint16_t i_mA;
  if (!parse_u16(i_tok, i_mA)) return CmdResult::BadArgs;
  Protocol proto;
  if (!parse_protocol(p_tok, proto)) return CmdResult::BadArgs;
  port->set_override(v_mV, i_mA, proto);
  return CmdResult::Ok;
}

CmdResult cmd_scenario(char* saveptr, MockPortReader* readers[3]) {
  CmdResult err;
  MockPortReader* port = resolve_port(saveptr, readers, err);
  if (!port) return err;
  char* id_tok = strtok_r(nullptr, kTokenSeps, &saveptr);
  if (!id_tok) return CmdResult::BadArgs;
  ScenarioId sc;
  if (!parse_scenario_id(id_tok, sc)) return CmdResult::BadArgs;
  port->set_scenario(sc);
  return CmdResult::Ok;
}

CmdResult cmd_status(MockPortReader* readers[3], char* out, size_t out_n) {
  if (!out || out_n == 0) return CmdResult::Ok;
  size_t off = 0;
  for (uint8_t i = 0; i < 3; ++i) {
    if (!readers[i]) continue;
    // Use the cached last reading so status does not advance the RNG
    // or re-evaluate the scenario from t=0.
    PortReading r = readers[i]->last_reading();
    int written = snprintf(out + off, out_n - off,
                           "P%u %s %umV C%umA A%umA %s\n",
                           (unsigned)i, r.attached() ? "on" : "off",
                           (unsigned)r.v_mV,
                           (unsigned)r.i_c_mA, (unsigned)r.i_a_mA,
                           protocol_name(r.proto));
    if (written < 0 || (size_t)written >= out_n - off) break;
    off += (size_t)written;
  }
  return CmdResult::Ok;
}

}  // namespace

const char* cmd_result_name(CmdResult r) {
  switch (r) {
    case CmdResult::Ok:         return "ok";
    case CmdResult::Unknown:    return "unknown";
    case CmdResult::BadArgs:    return "bad_args";
    case CmdResult::OutOfRange: return "out_of_range";
  }
  return "?";
}

CmdResult serial_cmd_dispatch(char* line, MockPortReader* readers[3],
                              char* out, size_t out_n) {
  if (out && out_n > 0) out[0] = '\0';
  if (!line) return CmdResult::Ok;
  char* saveptr = nullptr;
  char* cmd = strtok_r(line, kTokenSeps, &saveptr);
  if (!cmd) return CmdResult::Ok;  // empty/whitespace-only line

  if (!strcasecmp(cmd, "port"))     return cmd_port(saveptr, readers);
  if (!strcasecmp(cmd, "scenario")) return cmd_scenario(saveptr, readers);
  if (!strcasecmp(cmd, "status"))   return cmd_status(readers, out, out_n);
  return CmdResult::Unknown;
}

#ifndef UNIT_TEST

#include <Arduino.h>

namespace {

constexpr size_t kLineMax = 64;
char     g_buf[kLineMax];
size_t   g_len = 0;

}  // namespace

void serial_cmd_init(MockPortReader* readers[3]) {
  g_readers = readers;
  g_len = 0;
}

void serial_cmd_poll() {
  while (Serial.available() > 0) {
    int ch = Serial.read();
    if (ch < 0) break;
    if (ch == '\r') continue;
    if (ch == '\n' || g_len >= kLineMax - 1) {
      g_buf[g_len] = '\0';
      char status_out[128] = {0};
      CmdResult res = serial_cmd_dispatch(g_buf, g_readers,
                                          status_out, sizeof(status_out));
      if (status_out[0]) Serial.print(status_out);
      char tail[24];
      snprintf(tail, sizeof(tail), "> %s\n", cmd_result_name(res));
      Serial.print(tail);
      g_len = 0;
      continue;
    }
    g_buf[g_len++] = (char)ch;
  }
}

#endif  // UNIT_TEST
