# usb-charging-station

## Single source of truth

[docs/plan.md](docs/plan.md) is the canonical design document for this
project (system architecture, correction algorithm, protocol, pin
assignment, power budget, hardware notes). Read it before making any
cross-component decision. The English README intentionally duplicates only
the minimum needed for outside contributors; plan.md is the authority when
they disagree.

## Conventions

- Repository artifacts (README, code, comments) are English. Conversation
  and `docs/` may be Japanese.
- Code comments must not reference `docs/plan.md` (file path, section
  heading, or line number). plan.md is an internal/Japanese-language
  document and is not part of the public English source tree. Express
  the rule, invariant, or rationale in the comment itself; if a longer
  explanation is needed, point to the README or to a sibling source
  file. The same applies to Japanese phrases in code comments: keep
  source comments English-only.
- This repository is public. Do not commit personal hostnames, IPs,
  usernames, or LAN domain names. Per-machine values live in `.env`
  (gitignored); committed examples (`.env.example`, `.ssh-config.example`)
  use neutral placeholders.
- Do not write phase numbers, status, or implementation-progress notes
  ("Phase 0 done", "Status: WIP") in README or source. Such information
  rots quickly and the README is not the source of truth for state.
  plan.md tracks the design; git history tracks progress.

## Firmware build notes

- Do not add `-DUSE_TINYUSB` to `build_flags` for the Earle Philhower
  Arduino-Pico core. The core enables USB CDC by default; setting this
  macro breaks CDC enumeration so neither `picotool` nor the host serial
  driver can see the device after flashing.
