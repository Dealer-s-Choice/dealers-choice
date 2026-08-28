---
name: dc-testing
description: How to test Dealer's Choice against real running binaries — starting a headless server by hand, probing LAN discovery, reproducing a networking bug, and proving a code change actually reached the binary under test. Use when running or driving a live server/client/bot, investigating discovery or multi-process behaviour, or when a fix "does not work" and you need to rule out a stale process or a stale build first.
---

# Testing DC against live binaries

The meson suite covers game logic and the wire parsers. This skill is for what it
cannot reach: discovery on a real socket, multi-process behaviour, and anything
where you must watch a running server.

Paths here are **repo-relative**. Substitute your own build directory
throughout — this project is usually configured with several at once (a plain
one, ASan, TSan, gcc), so there is no single correct name. List them with:

```sh
ls -d */meson-info 2>/dev/null | xargs -n1 dirname
```

## Three traps that invalidate a run

Each of these produced a confidently wrong conclusion in one session (issue #368).
They share a shape: **the test runs, prints plausible output, and is not measuring
the thing you changed.** Rule all three out before believing a result — especially
a result that says your fix did not work.

### 1. A stale server answers for the one you just started

Backgrounded servers outlive the command that launched them. The next one fails
with `Address already in use`, exits, and your probe is answered by the *previous
binary* — the one without your fix.

Check before, and prove liveness after:

```sh
pgrep -af dealers-choice-server || echo "none running"

nohup ./dealers-choice-server --port 22999 > /tmp/dc-srv.log 2>&1 &
sleep 2
pgrep -af dealers-choice-server | head -2
head -5 /tmp/dc-srv.log      # a bind error means you are testing a ghost
```

`--port` does **not** move LAN discovery, which binds the configured discovery
port. Two servers on different game ports still collide there, and the first one
keeps answering.

### 2. `pkill -f dealers-choice-server` kills your own shell

The pattern matches the agent shell's own command line, which contains that
string. The call dies with **exit code 144** (128+SIGTERM) and everything after
the `pkill` silently never runs — including edits you expected it to make. An
exit 144 from a command containing `pkill` is this.

**`pkill -x` is not the fix.** Linux matches `-x` against the 15-character
process name, and `dealers-choice-server` is 21, so it warns and kills nothing —
which reads as "no strays" while the stray keeps running. Anchor a `-f` pattern
so it cannot match the agent shell, and kill by pid:

```sh
pgrep -f "^\./dealers-choice-server" | xargs -r kill
```

**Anchoring is not enough when the pattern itself appears in your command.**
`pkill -f "scripts/soak.sh"` dies the same way, because the shell running it has
`scripts/soak.sh` in its own argv. Anchoring only helps for patterns starting
`^./`, which a shell's argv never does. For anything else — a script name, a
path fragment — **split it across two tool calls**: `pgrep` for the pid in one,
`kill <pid>` in the next. A literal numeric pid cannot match anything but the
process you meant.

This one recurred three times in the session that first documented it, twice
*after* it was written down, because the failure is invisible: the command exits
144 and everything after the kill — including source edits in the same call —
silently never happens. If a tool call containing a kill exits 144, assume none
of its later steps ran and re-check the files it was supposed to change.

To see what is running, `pgrep -af dealers-choice` and read past your own shell
in the output.

### 3. Suppressed build output hides whether the binary changed

`meson compile … | grep -i error ; echo "build ok"` prints "build ok" regardless:
`;` runs the echo unconditionally and a grep matching nothing exits 1. Editing
source and then testing a stale binary is indistinguishable from a fix that does
not work.

Prove the code reached the binary rather than inferring it:

```sh
ls -l --time-style=+%H:%M:%S <builddir>/dealers-choice-server; date +%H:%M:%S
strings <builddir>/dealers-choice-server | grep -c MY_MARKER
```

An unchanged mtime after a compile is itself information: either the edit did not
land, or it was to a file this target does not build.

## Temporary instrumentation

When behaviour disagrees with the source, print from the line in question instead
of reasoning about scope. In #368 a one-line `fprintf` settled in a single run
what two readings had not — it printed **zero** times, which is what exposed the
ghost process.

```c
fprintf(stderr, "XXX generated lan_instance_id=%08x\n", lan_instance_id);
```

Prefix the marker so removal and verification are mechanical: `sed -i '/XXX /d'`,
then `grep -rc XXX src/` for the source **and** `strings … | grep -c XXX` for the
binary. Both, before committing.

## LAN discovery probe

`lan-discovery-probe.py` beside this file sends real discovery queries and prints
each reply's source address, TCP port and `instance_id`, then counts distinct ids.
It needs no client build and no display.

```sh
python3 .claude/skills/dc-testing/lan-discovery-probe.py [discovery_port]
```

Reading the output:

- **Several source addresses, one id, same round** — correct. A server answers
  once per interface (IPv4 broadcast, IPv6 link-local multicast, loopback) and the
  client collapses them in `lan_upsert` (`src/ui/menus.c`), keyed on `instance_id`
  and preferring the most connectable address.
- **Different ids across rounds from one server** — the server is rerolling its
  identity, which is #368. The id must be generated once per process, outside the
  lobby loop in `run_server`.
- **Passing condition: distinct ids == number of real server processes.** Count
  the processes first; two test servers legitimately give two.

The wire format is documented at the top of `src/net/lan_discovery.c` and the
probe hardcodes it, so a format change needs the same edit in both.

## Running the binaries

Run from inside the build directory, which resolves `../data` for you:

```sh
cd <builddir>
./dealers-choice-server --port 22999 --verbose
./dealers-choice-bot --host 127.0.0.1 --port 22999
```

Server flags worth knowing: `--debug` (per-opcode trace), `--log-file PATH`,
`--disable-lan-discovery` (do not answer probes on a real network),
`--disable-publish` (undocumented but implemented; scripted server spawns pass
it), `--autodeal` and `--disable-timeout` for unattended runs.

To exercise the **installed** layout — which no CI job builds, since every CI
`meson setup` passes `-Dregistry=true` — install to a scratch destdir:

```sh
meson install -C <builddir> --destdir /tmp/dc-inst
DEALERSCHOICE_DATADIR=/tmp/dc-inst/usr/local/share/dealers-choice ./dealers-choice-server
```

`common.conf` is not installed, so that also exercises the absent-file branch in
`get_common_registries`.

## Match the test to the change

The full suite is 57 tests and many are multi-second socket tests, so running all
of it after a comment or logging change is wasted time. Run what the change can
break:

```sh
meson test -C <builddir> test_registry          # one test
meson test -C <builddir> --suite dealers-choice # everything
```

Reserve the full run for changes to the server loop, the wire protocol, or
anything touching shared state — and for the point just before committing. A
build alone (`meson compile`) is enough to check that a comment or docs edit did
not break anything.

## Longer runs

`scripts/soak.sh` drives bots against a server for several minutes; run it after
server-side changes and **read the log**, not just the exit status. Background
long runs and tee to a log file so progress can be watched with `tail -f`.

## Before claiming a result

- The server you probed is the one you built: pid checked, log free of bind errors.
- The binary is newer than the edit, or contains your marker.
- No servers from earlier runs are still listening.
- You have the **before** measurement, not only the after. "Five different ids
  before, one after" is a result; "it looks right now" is not.

## Keep this skill current

**Update this file yourself, without being asked, whenever a session teaches you
something about testing DC.** Do not wait to be told and do not ask permission — a
lesson that lives only in a finished conversation is lost. Edit, then mention the
edit in one line.

Correct and improve it as well as extend it: an entry that turns out to be wrong,
or that describes a flag, path or function that no longer exists, is worse than no
entry, because it will be trusted. Verify a claim before relying on it here, and
fix it in place the moment you find it stale.

The triggers, concretely:

- **A test run misled you.** Any case where output looked valid but measured the
  wrong thing belongs in "Three traps" — that is what the section is for, and it
  is the highest-value thing this file carries.
- **You worked out how to reproduce something awkward** (a race, a
  platform-specific failure, a multi-process setup). Write down the recipe while
  it still works.
- **A command here fails or a path has moved.** Fix it in the same session rather
  than working around it.
- **A new binary, flag, or harness appears**, or an existing one changes meaning.
- **Something here proves to be a false alarm.** Weaken or remove it; a rule that
  gets ignored teaches the next reader to ignore its neighbours.

Rules for the edit: sharpen an existing entry rather than appending a near
duplicate, keep the concrete measurements and issue numbers since they are what
makes a trap recognisable later, and keep it short enough to be re-read.
