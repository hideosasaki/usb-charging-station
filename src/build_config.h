// Compile-time switches that select between mock and real hardware
// implementations. Defaults are tuned so a bare `pio run` builds the mock
// firmware suitable for display-side development without SW3518 hardware.

#pragma once

#ifndef USE_MOCK_PORTS
#define USE_MOCK_PORTS 1
#endif
