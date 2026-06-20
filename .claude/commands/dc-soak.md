---
description: Short (~5 min) sanitized bot soak of the server (run after server-side changes)
allowed-tools: Bash
---
Run a short sanitized soak of the server (sustained play + bot churn + the
0-player showdown path, ASan/UBSan armed). Needs `_build_asan` built first — run
`/dc-verify` or `meson compile -C _build_asan` if it isn't.

Run it backgrounded (the phases take minutes):

```sh
DC_BUILD=$PWD/_build_asan DC_PORT=24905 DC_PASSWORD=soak \
  P1_BOTS=2 P1_MIN=3 P2_BOTS=3 P2_MIN=2 P3_ROUNDS=3 \
  LOG=/tmp/dc_soak.log SRVLOG=/tmp/dc_soak_srv.log \
  bash scripts/soak.sh
```

**Judge it by the log, not the exit code.** A real pass ends with
`DONE: all phases passed` in `/tmp/dc_soak.log`. Then confirm the server log is
clean: `grep -ic 'runtime error\|AddressSanitizer' /tmp/dc_soak_srv.log` should
be 0. (The script has reported exit 0 while the server never started, and
backgrounding can surface a harmless non-zero code — so always read the log.)
