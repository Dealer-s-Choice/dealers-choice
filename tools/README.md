# tools/

Standalone diagnostic command-line programs for Dealer's Choice. You run them
by hand against a running system. They are not part of the test suite and are
not installed.

These are off by default. Build them with the `tools` option:

    meson setup _build_dealers_choice -Dtools=true -Dgen_protobuf=true
    meson compile -C _build_dealers_choice

Release and package builds leave `tools` off, so the diagnostics are never
shipped.

## How this differs from `scripts/` and `tests/`

- `tests/` — programs run by `meson test`.
- `scripts/` — shell and Python harnesses that drive the test suite, CI, and
  release steps (for example `scripts/soak.sh`). See `tests/README.md`.
- `tools/` — standalone diagnostic programs you run yourself, like the ones
  below.

## registry-probe

Talks to a registry (directory server) over TCP. Use it to read a registry's
server list, or to send a test announce.

    registry-probe list     <reg_host> <reg_port>
    registry-probe announce <reg_host> <reg_port> <game_port> [name]

`list` prints the servers the registry currently knows about. `announce`
registers a fake server entry, which is useful for testing the registry by hand.

It links only the SDL-free core (`libdc_core`), so it builds without SDL.
