// Line-oriented Serial command parser. The free dispatch function is
// host-test-buildable; serial_cmd_poll() is the Arduino-only glue.

#pragma once

#include <stddef.h>
#include <stdint.h>

class MockPortReader;

enum class CmdResult : uint8_t {
  Ok,
  Unknown,
  BadArgs,
  OutOfRange,
};

const char* cmd_result_name(CmdResult r);

// Parses one command line in-place (the buffer is modified by strtok-style
// splitting). `out` receives a caller-supplied scratch buffer for the
// `status` response; pass nullptr / 0 to suppress.
CmdResult serial_cmd_dispatch(char* line, MockPortReader* readers[3],
                              char* out, size_t out_n);

void serial_cmd_init(MockPortReader* readers[3]);
void serial_cmd_poll();
