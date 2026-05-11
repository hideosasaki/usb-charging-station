# Vendored: SW35xx_lib

Upstream:    https://github.com/dimitar-grigorov/SW35xx_lib
Backup fork: https://github.com/hideosasaki/SW35xx_lib
Commit:      6253d660f454a936449163307cbe7d5369038278
Fetched:     2026-05-11
License:     MIT (see LICENSE)

## Why vendored

Depending on the upstream repository via `lib_deps` would make the build
break the moment upstream is moved or deleted. This project is a public
repository where reproducibility matters, so the minimum source files
needed are copied into `src/` under this directory. Upstream activity has
effectively plateaued and the register-read logic this driver needs is
stable, so the cost of not tracking upstream is near zero. A personal
fork is kept at the URL above as insurance against upstream deletion.

## Files included

- `src/I2CInterface.h`  — pure-virtual I²C base class
- `src/SW35xx_lib.h`    — SW35xx driver declarations (namespace SW35xx_lib)
- `src/SW35xx_lib.cpp`  — driver implementation
- `LICENSE`             — MIT license text
- `library.properties`  — PlatformIO / Arduino IDE metadata

## Files intentionally omitted

- `src/TwoWireWrapper.h`      — replaced by `src/wire_i2c_wrapper.h`,
                                which also pins SDA/SCL/clock at begin()
- `src/SoftwareWireWrapper.h` — depends on SoftwareWire, not used
- `src/SerialUtils.h`         — debug-only serial_printf, not used
- `src/SimpleTest.cpp`        — example sketch; must be excluded from the
                                library build
