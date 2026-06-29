# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

Run from the `dealers_choice/` directory:

```sh
meson setup _build_dealers_choice --reconfigure -Dgen_protobuf=true
meson compile -C _build_dealers_choice
```

All `meson compile`, `meson test`, and `meson devenv` commands must use `-C _build_dealers_choice`. Never use `_build` or `build`.

To update subprojects after upstream changes:

```sh
meson subprojects update
```

## Tests

**See [`tests/README.md`](tests/README.md) for the full testing guide** — the
meson suite (unit + game-logic groups), the `scripts/` harnesses and
`scripts/soak.sh`, sanitized builds for live verification, `tc netem` impairment,
heaptrack profiling, running the binaries by hand (data-dir / bot-password /
`gen_protobuf` gotchas), screenshotting the GUI, and troubleshooting.

```sh
meson test -C _build_dealers_choice -v
```

Quick reminders (details in `tests/README.md`):
- All `meson` commands use `-C _build_dealers_choice` (see Build); never
  `_build` / `build`.
- The CI gate is **gcc `-Werror`**, but the ASan build is **clang** — do a gcc
  pass before calling a branch CI-ready (clang stays silent on
  `-Wformat-truncation` and the like).
- After **server-side** changes, run a short `scripts/soak.sh` and judge it by
  the log (`DONE: all phases passed`), not the exit code.

### Claude Code commands

Project slash commands live in `.claude/commands/`. They are discovered **only
when Claude Code is launched from the repo root** (`dealers_choice/`) — starting
from a parent dir means `/dc-*` won't resolve (`cd`-ing in afterward doesn't fix
it; the project root is fixed at launch). Available:

- `/dc-verify` — gcc `-Werror` gate + clang ASan/UBSan build + full meson tests.
- `/dc-soak` — short (~5 min) sanitized bot soak (run after server-side changes).
- `/dc-gate` — pre-merge gate: runs `/dc-verify` then `/dc-soak`, one PASS/FAIL.

## Architecture

**Client-server** networked game, split across three binaries that talk over
TCP: the GUI client (`dealers-choice`), a headless server
(`dealers-choice-server`), and a headless bot (`dealers-choice-bot`). The GUI is
a **client only** — it no longer hosts a server (the old `--server` mode is a
deprecation stub; see "headless server binary name" below). Sources live under
`src/{game,net,server,ui}/` by area, with the SDL-free logic in `libdc_core`.

### Static library chain

Meson builds the local libs bottom-up:

1. **tcpme_lib** (`src/tcpme/`) — cross-platform TCP socket abstraction (POSIX + Winsock). Exposed as `tcpme_dep` and merged into `shared_dep`. Note: tcpme was written almost entirely by an LLM, which is a potential concern for correctness and security.
   - **Keep tcpme generic and DC-agnostic.** It is meant to be a standalone, reusable library (SDL_net-style), so it provides *mechanism* only; DC provides *policy*. Do not put DC-specific values, constants, comments, or assumptions in `src/tcpme/`. Example: socket I/O timeouts — tcpme exposes `tcpme_set_timeout(sock, ms)` (the generic primitive); DC owns the value (`SOCKET_IO_TIMEOUT_MS` in `net.h`) and applies it after connect/accept. When a fix needs a tunable or a project-specific decision, expose a knob in tcpme and set it from DC, don't hardcode it in the library.
2. **`libdc_core`** (`dc_core_dep`) — the **SDL-free** core: game rules, protocol/serialization, and the server engine (`common_src` = `src/{game,net,server}` + core root files). No SDL, ttf, image, or audio. Linked by all three binaries.
3. **`_lib` ("game")** (`game_dep`) — the GUI library: rendering, widgets, menus (`gui_src` = `src/ui/` + `widgets_src`), adding SDL2/ttf/image + miniaudio on top of `dc_core_dep`.

Binaries: `dealers-choice` = `main.c` + `game_dep`; `dealers-choice-bot` = `bot.c` + `dc_core_dep`; `dealers-choice-server` = `src/server/server_main.c` + `dc_core_dep`.

**Why the split:** the bot and server binaries must link **no SDL at all**, so anything pulling in SDL/ttf/image/audio belongs in `src/ui/` (the GUI lib), never in `libdc_core`. When adding a source file, decide: shared game/protocol/server logic → an area dir under `src/{game,net,server}`; GUI-only → `src/ui/` (`ui_src`) or `src/widgets/`.

### Key source files

| File | Role |
|---|---|
| `src/main.c` | GUI entry point: SDL init, top-level menu, connection loop |
| `src/cli.c` | Shared CLI parsing — `parse_cli_args` (client) + `parse_server_args` (server) |
| `src/ui/client.c` | GUI client: connection lifecycle, audio threads, SDL setup/teardown |
| `src/ui/game_logic.c` | Gameplay-screen render + input loop (`handle_game_logic`) |
| `src/ui/game_select.c` | Pre-game lobby / game-selection screen |
| `src/ui/menus.c` | Connect / settings / hotkeys menu screens |
| `src/server/server.c` | Server run loop, lobby, client management, messaging |
| `src/server/round.c` | Betting / draw / showdown engine |
| `src/server/variants.c` | Poker variants + per-hand play orchestration (`play_game`) |
| `src/server/server_main.c` | Headless `dealers-choice-server` entry point |
| `src/net/net.c` | Protobuf-c serialization/deserialization, magic-byte framing |
| `src/game/game.c` | Poker variant definitions + shared game model |
| `src/bot.c` | Headless bot (links `libdc_core` only) |
| `src/dc_config.c` | canfigger-based config file handling |
| `src/types.h` | Canonical type definitions (`Player_t`, `GameState_t`, `EPlayerAction_t`, etc.) |

### Networking

- Protocol: custom binary framing ("DCPROTO" magic + version + flags) wrapping protobuf-c messages.
- Version constant: `GAME_PROTOCOL_VERSION` in `src/net/net.h`. **Andy owns this bump — never change it yourself.** He bumps it whenever the wire protocol changes (a new game/opcode, a new field in the protobuf schema, framing changes), not on a fixed per-release schedule; expect it to change less often as the protocol stabilizes.
- Default port: 22777.
- Auth: libsodium-based password hashing.
- **Registry (directory server).** `dealers-choice-server` announces itself to,
  and the GUI client browses, the registries in `data/common.conf` (default: the
  public `registry.dealers-choice-foss.dev`, TCP 22070). The
  `dealers-choice-registry` binary (`src/registry/registry_main.c`) verifies each
  announce by connecting back, expires stale entries (TTL), and writes
  `servers.json`. Client opt-out: `registry_browser = no` in player.conf /
  `--disable-registry-browser`. Server opt-out: `--disable-publish` or the
  `DC_DISABLE_PUBLISH` env (the meson tests set it so test servers never publish
  to the live registry). Details in `docs/REGISTRY.md`; Docker in
  `docker/README.md`. Open registry work: #74 (non-blocking verify), #75 (reload
  `servers.json` on boot + faster announce retry).

### Website / server-list widget (`web/`)

`web/server-list.html` is a self-contained, framework-free page (inline CSS/JS,
no external deps) that fetches a registry's `servers.json` and renders a live,
auto-refreshing table of active servers. The felt theme uses `web/felt.png` (a
seamless green tile cut from the game's `data/images/felt.png`, kept in sync).

- **Served by Caddy, not Apache/nginx.** `web/docker-compose.yml` runs the
  official Caddy image; `web/Caddyfile` serves everything from `/site` and
  `/servers.json` from a separate read-only `/json` mount (same origin, so the
  page needs no CORS). Auto-HTTPS via Let's Encrypt, TLS-ALPN-01, **port 443
  only** (no port 80). Config via `.env`: `DC_WEB_DOMAIN`, `DC_WEB_ROOT`.
  `DC_WEB_DOMAIN=localhost` makes Caddy use its built-in local CA (no LE call).
- **Local dev loop:** drop a sample `servers.json` (git-ignored) and
  `python3 -m http.server` — no Docker/cert/registry needed; the page re-fetches
  every 30s. Validate Caddyfile edits with `caddy validate`.
- **Hardening = Caddyfile, not `.htaccess`.** Caddy has no `.htaccess`; the
  Apache/nginx 8G / bad-bot rulesets don't apply. It's a static, no-login site,
  so the realistic threats (bad-UA scanners, exploit-path probes like
  `/.env` `*.php`, non-GET methods) are blocked with `@matcher` + `abort` /
  `respond` directives in the Caddyfile. `robots.txt` is advisory only, not a
  control.
- **CrowdSec, if ever needed:** don't rebuild Caddy with the bouncer module for
  this static site — turn on Caddy JSON access logs to a mounted file, run the
  CrowdSec agent on the host with the `crowdsecurity/caddy` collection, and a
  host firewall bouncer. Save heavier IPS for the boxes running attackable
  services (SSH, the game/registry servers).

### Audio

miniaudio is used for sound effects. Both `ma_engine_init` and `ma_engine_uninit` can block for several seconds when a sound server (e.g. PulseAudio) is slow or a USB audio device is involved. Both run on background SDL threads (`audio_init_thread_fn` / `audio_uninit_thread_fn` in `src/ui/client.c`) while the main thread pumps events to keep the window responsive. Do not move either call back to the main thread.

#### Audio device-change handling — in-progress work

**Confirmed fixed:** When the user switches audio devices mid-session, `ma_device_uninit` used to block forever because the PA device worker thread was stuck in `pa_mainloop_iterate(block=1)`. Fix: `audio_uninit_thread_fn` calls `ma_engine_stop` before `ma_engine_uninit`; this routes through `ma_device_data_loop_wakeup__pulse` → `pa_mainloop_wakeup`, breaking the blocked iterate. Do not revert this.

**Unresolved: no sound after device switch / on startup.**

**Update (2026-06-21): likely hardware, not DC.** Since switching to a new
Logitech USB wired headset (~2 weeks prior), the no-sound problem has **not
reproduced**. This strengthens root cause #1 (the old C-Media chip on a marginal
USB port) over any DC software bug. The restart machinery below has not been
needed since; it remains a candidate to strip back to the freeze-fix-only state.

Two possible root causes had been identified — they may both have been in play:

1. **Marginal USB port putting the C-Media sink in a bad state.** The C-Media USB audio chip (0d8c:0012) was enumerating cleanly (`dmesg` showed no errors, `power/control = on`) but PA's sink was going `SUSPENDED` and not resuming correctly, causing DC and browser video to stall when trying to open audio streams. Switching the headset to a different USB port fixed it immediately; switching back to the original port continued to work. The port had a marginal connection that was good enough to enumerate but caused intermittent PA sink state corruption.

   **System config in place** (good hygiene regardless of root cause):
   - `restore_device=false` in `~/.config/pulse/default.pa` — prevents module-stream-restore from pinning streams to a remembered sink
   - `module-suspend-on-idle` commented out in `~/.config/pulse/default.pa` — prevents PA from auto-suspending idle sinks (note: `timeout=0` means "suspend immediately", not "never suspend"; commenting out the module entirely is the correct way to disable auto-suspend)

2. **`module-stream-restore` sink routing** — ruled out. `restore_device=false` is confirmed active (`pactl list modules short | grep stream-restore` shows `restore_device=false`). The config is in `~/.config/pulse/default.pa` and is being picked up. This is no longer a suspect.

**Current state of the restart code in `src/ui/client.c`:** There is substantial `audio_restart_thread_fn` / `maybe_restart_audio` / `g_audio_missed_notification` machinery that attempts a full engine+sounds uninit/reinit when PA sends `rerouted` or `stopped` notifications. This code is unreliable against active `module-stream-restore` (PA moves the new stream again immediately after reinit) and has introduced its own bugs (cleanup hang when `ma_engine_init` blocks in the restart thread). Consider stripping this back to the freeze-fix-only state once the root cause is better understood.

### Subprojects

`canfigger` and `miniaudio` are Meson subprojects under `subprojects/` (canfigger via a pinned `.wrap`); changes to them are committed in their own upstreams, not the main repo. `pokeval` and `deckhandler` were moved **in-tree** (`src/pokeval`, `src/deckhandler`) as of 0.0.14 — they are **not** subprojects, and their upstream repos are archived, so edit and commit them directly in the main repo.

## Styling system — `style.h` / `style.c`

> **STATUS (2026-06-19): planned, NOT yet implemented.** On trunk, `src/ui/style.c`
> is ~53 lines of hardcoded `SDL_Color` constants; there is no `[styles]` section in
> `data/layout.conf`, no `StyleConfig_t`/`g_style_cfg`/`get_style_config`/`style_fields`,
> and no `parse_color`. The design below is the target (task #66) — do not assume these
> symbols exist.

Widget colors and font choices are configured in the `[styles]` section of `layout.conf` and parsed at startup into `g_style_cfg`. **Do not use libcss or any external CSS library** — CSS is designed for flow-based document layout and does not map to an absolute-positioned SDL2 UI.

### Files

| File | Role |
|---|---|
| `src/ui/style.h` | `StyleConfig_t` typedef, `extern StyleConfig_t g_style_cfg`, `get_style_config()` declaration |
| `src/ui/style.c` | `get_style_config()` implementation, `parse_color()`, `parse_font()`, offset-table dispatch |
| `data/layout.conf` `[styles]` | Per-role color/font values read at startup |

`style.h` is included via `globals_gui.h` (the GUI umbrella header), so it is available everywhere `globals_gui.h` is included. (`globals.h` itself is SDL-free since the renovation and no longer pulls in `style.h`.) Files outside that chain (e.g. `graphics.c`, `widgets/ping.c`) include `style.h` directly.

### `StyleConfig_t` fields

Color-pair roles (each has `.bg` and `.fg`): `button_primary`, `button_danger`, `button_warn`, `button_cancel`, `indicator_wild`, `indicator_game`.

Single-color roles: `text_on_dark`, `text_on_light`, `text_muted`, `link_normal`, `link_hover`, `timer_bg`, `timer_elapsed`.

Font roles (store `FONT_*` index): `button_font`, `link_font`.

### `layout.conf` format

Color-pair keys use canfigger's attribute syntax — value is the bg color, first comma-separated attribute is the fg color:

```ini
button_primary = black, yellow
indicator_wild = purple, white
```

Single colors and font names are plain values:

```ini
text_on_dark = white
timer_elapsed = #F0CC30
button_font = bold
```

Color strings: named (`white`, `black`, `table_green`, `orange`, `purple`, `brown`, …) or hex `#RRGGBB` / `#RRGGBBAA`. Font strings: `bold`, `default`, `link`, `title`, `version`, `status_msg`, `card`, `default_bold`, `wild_select`.

### Dispatch — offset table in `style.c`

The parser uses a static `style_fields[]` table (key, `FieldType_t`, `offsetof`, default values). Adding a new style field is one table row — no `else if` chain to maintain.

### Widget creation

- `button_widget_create_colored(text, SDL_Color bg, SDL_Color fg, font, hotkey)` — SDL_Color entry point alongside the existing `EColor_t` version.
- `create_indicator_colored(text, font, SDL_Color bg, SDL_Color fg)` — likewise for indicators.
- Call sites pass `g_style_cfg.button_primary.bg` / `.fg` etc. directly.

### What stays out of config

- Card back pattern colors (intentional per-style literals in `card_back_styles[]`)
- Colors computed at runtime (animation tints, alpha blends, the circle timer's 3D border gradient)
- Logical dimensions (`LOGICAL_WIDTH` / `LOGICAL_HEIGHT`) — compile-time constants

## Code conventions

- **String duplication:** use `dc_strdup()` (`src/util.c`) in files that don't already include SDL headers. Use `SDL_strdup` only where SDL is already included. Never use POSIX `strdup` — it's unavailable on MSVC.
- **C standard:** C11 (`-std=c11`), warning level 3, with `-Werror` on aliasing, ODR, LTO type mismatches, and int conversion.
- **Portability:** code runs on Linux, macOS, Windows (MSVC), and BSD. Platform guards use `host_sys` / `_WIN32`.
- **i18n:** user-visible strings go through gettext macros (see `src/translate.h`:
  `_()` for translate-now, `N_()` to mark a string for extraction that is
  translated later at the point of use — the pattern used by the hotkey tables in
  `src/ui/hotkey_table.c`, rendered through `_()` in `src/ui/hotkey_overlay.c`).
  Translation `.po` files live in `po/`.
    - **A new `.c` with `_()`/`N_()` strings must be added to `po/POTFILES`** or
      its strings are silently never extracted (they compile and display in
      English but can't be translated). This bites whenever a file is split out
      or added — check `po/POTFILES` whenever you introduce marked strings in a
      new file.
    - **Follow GNU gettext's "Preparing Strings" advice**
      (<https://www.gnu.org/software/gettext/manual/html_node/Preparing-Strings.html>):
      mark complete self-contained units (no runtime concatenation of
      fragments into a sentence), never pass a computed/runtime value through
      `_()` (e.g. an SDL key name stays un-translated), and add a
      `// TRANSLATORS:` comment above short or ambiguous strings for context
      (xgettext is run with `--add-comments=TRANSLATORS`; use `//` line comments,
      not `/* */`, so a stray `*` doesn't leak into the `.po`). Key-cap tokens
      ("F1", "Esc", "Alt+Enter", "1-5") are intentionally left un-marked.
    - **Don't commit a regenerated `.pot`/`.po` as a side effect** — Andy commits
      `po/` updates in their own dedicated commits; the build regenerates the
      `.pot` on demand.
- **Comments are welcome on this project.** Andy maintains Dealer's Choice and wants explanatory comments kept — including ones written to orient a future AI session, not just human readers. Favour comments that capture the *why* (a hidden constraint, a prior bug, a surprising invariant, why an approach was chosen) over ones that merely restate the code. Don't pad with noise, but don't strip useful context to hit a "rare and short" bar — that bar applies to other people's projects, not this one. **Public API docs** — doxygen/header docblocks on public functions, parameters, and return codes — can be as long as needed; that's contract documentation for callers.

## Working style & contributions

Applies to human contributors and to any LLM/AI assistant used on this project:

- **No sycophancy.** Don't agree just to be agreeable. Take an idea seriously, push back when something looks wrong, and propose the better alternative.
- **ChangeLog: one line per entry** (two only if a single line genuinely can't carry the meaning). The diff and the linked PR/issue hold the detail — don't restate the change.
- **LLM-drafted public text gets a short preface.** When an AI assistant writes content posted under a contributor's account — GitHub issues / PRs / comments, or a commit-message *body* (one-line subjects need none) — it should identify itself as an LLM (with model/version), note it was posted at that contributor's direction, write in its own voice (first person for what the tool did; the contributor's plain name — never an `@`-mention — for what they did), avoid anthropomorphic phrasing, and flag anything it hasn't verified. A contributor's own assistant config may set the exact format; this is just the project default.

## UI / SDL2 menus

SDL2 is a rendering/input layer, not a widget toolkit, so every screen (in
`src/ui/menus.c`, `game_select.c`, `game_logic.c`; `main.c` drives the
top-level loop) is a hand-rolled `while (running) { poll events; render; }` loop.
The `ui_widget`/`UIRegistry` abstraction (auto render/destroy) and
`button_widget_create_styled` soften it, but each menu still repeats a lot of
boilerplate. Two refactors worth doing once there are ~4–5 of these screens
(don't bolt on mid-feature):

- **Use the existing `ui_table_begin/add/layout` helper** (`ui_widget.h`) for
  row/column screens instead of hand-computing `x/y`. `menu_display_hotkeys`
  positions its label+box rows manually and is a candidate.
- **Shared "menu event loop" helper** to handle quit / Escape / back-arrow /
  F11 fullscreen once, instead of every screen copying that block.
- **`menu_display_settings` is fragile and full** (#76). It hardcodes positional
  config indices (`bool_idx`, `password_idx`) and assumes a single bool checkbox;
  the 2×3 grid is full, so a second bool (`registry_browser`) had to be
  special-cased into the third column. Redesign: drive widgets from
  `player_config_entries[]` by type (checkbox per `CFG_TYPE_BOOL`), drop the
  positional hardcoding, and use a scalable (scroll/paginated) layout.

A heavier framework (Dear ImGui, etc.) is **not** the answer — it fights the
absolute-positioned style and the `style.h` config system.

### Configurable game-play hotkeys (branch `claude/configurable-hotkeys`)

Action hotkeys (check/bet/fold/call/raise/complete/discard) live in
`player.conf` as `hotkey_*` string entries (`CFG_GROUP_HOTKEYS` in
`player_config_entries[]`), resolved to keycodes at startup by
`src/hotkeys.c` into `g_hotkey_cfg`. The grouping keeps them off the main
Settings grid; they're edited on a press-to-bind sub-screen
(`menu_display_hotkeys`) reached from a "Hotkeys" button on Settings.
Card-selection and bet-amount digit keys (`1`–`8`) are intentionally
hard-coded — they're positional, not semantic.

**Follow-up idea (not yet built): F1 in-game opens the hotkeys screen.**
Reusing `menu_display_hotkeys` for display is easy, but two caveats before
building it: (1) it's a *blocking* modal, so opening it mid-hand freezes the
table view while the server action timer keeps counting — risk of timing out
on your turn; (2) action buttons bake in their hotkey at creation
(`button_widget_create_styled(..., action_hotkey(i))`), so a mid-game rebind
won't take effect until the buttons are rebuilt unless the in-game key
handler is changed to read `g_hotkey_cfg` live. Prefer treating F1 as a
read-only reference overlay, with editing left to the Settings path — or do
the live-`g_hotkey_cfg` change if in-game editing is wanted.

## Known issues — open on branch `claude/automated-bot-testing`

Surfaced by `tests/test_pokeval_fuzz` (registered in `tests/meson.build`) and the harness in `scripts/disconnect_test.sh`. The fuzz test currently fails CI on this branch because it pins behavior that is still wrong; pick these up before merging.

### 1. Pokeval wild straight-flush / flush tie-breaks ~~(~2 mismatches per 1000 fuzz hands)~~ — FIXED

Fixed by introducing `compare_wild_same_rank` in `src/pokeval/pokeval.c`, which centralizes the wild-aware tie-break dispatch and is shared between `update_best_wild` (best-5-of-7 selection) and `POKEVAL_compare_hands_wild` (cross-player winner determination).  The old code had each call site fall back to `compare_high_cards` on the raw combo, which left wilds at `face_val == wild_face` and mismeasured anything past HIGH_CARD.  50,000 fuzz hands across 5 seeds now pass with 0 mismatches; `tests/pokeval_fuzz_check.py` runs the full variant set (no more `--skip-wild`).


`POKEVAL_compare_hands_wild` already has rank-aware tiebreaks for FIVE_OF_A_KIND / FOUR / FULL_HOUSE / THREE_OF_A_KIND / PAIR / TWO_PAIR (the last two added in commit `pokeval: wild-aware tie-break for PAIR and TWO_PAIR`).  STRAIGHT, STRAIGHT_FLUSH, and FLUSH still fall through to the `default:` case, which calls `compare_high_cards` on the raw combo — and the raw combo has wilds at `face_val = 2`.  When two flushes / straight flushes tie at the same rank, the wild card sorts to the bottom of the hand instead of being treated as its substituted face, so a Q-high wild SF can lose to a 5-high wheel SF (or a wild flush gets miscompared against a natural one).

Concrete failure case from the fuzz harness:
```
P1: Js 2h 9c 8c Tc 2c Qd   — Q-high straight flush in clubs (wilds = Jc, Qc)
P2: Kh 5h 2s 6s 4h Ah 2d   — 5-high wheel straight flush in hearts (wilds = 2h, 3h)
```
Pokeval picks P2 because `compare_high_cards` on the raw combos walks `10 9 8 2 2` vs `14 5 4 2 2` and `14 > 10`.

Fix sketch: add `POKEVAL_STRAIGHT`, `POKEVAL_STRAIGHT_FLUSH`, and `POKEVAL_FLUSH` cases to the `compare_hands_wild` switch.  For straights, substitute the wild into the missing-rung position(s) and compare the resulting straight-high values (handle the A-2-3-4-5 wheel the same way `update_best_5card` already does at line ~819).  For flush, substitute each wild to the dominant suit and pick the highest non-duplicate face for each, then compare high-down.

### 2. ~~Bot reconnect handshake silently hangs ~47% of the time~~ — RESOLVED

Originally observed on the 60-min disconnect harness: roughly half of bot reconnect attempts would print `Exchanging protocol information...` and stop, with no error and no follow-up `Connected to…`.  Re-running the same harness after the server stack-use-after-return fix (`server: clear starting_turn after play_game to avoid stack-use-after-return`) returned **44/44 successful reconnects** across 4 bots in 60 minutes.

Root cause was downstream of the UAR: when the server dereferenced freed stack memory in `args->starting_turn` during the post-game wait window, the corrupted state plausibly stalled `register_new_client` for queued accept-queue entries, so the bot would sit in `recv_all_tcp` waiting for the server's 1-byte ACK that never came.  No tcpme-level bug; the pattern disappears when the server doesn't reach into freed memory.

### 3. Bot wire format leaks ACE_HIGH (face_val = 14) when not using the GUI client

`POKEVAL_sort_hand` mutates aces from `DH_CARD_ACE` (1) to `POKEVAL_ACE` (14) in place so its straight detector sees them at the top of the sort.  Commit `server: restore ace face_val before broadcasting sorted hand` (server.c:`handle_sort_hand`) restores the value before the broadcast, but the underlying pokeval design still couples the sort to evaluator-internal numbering.  Worth considering a cleaner pokeval API split: a pure `POKEVAL_sort_hand_for_eval` that callers know is destructive, vs. a display-safe sort.

## Open for discussion — headless server binary name

The headless server binary added during the renovation is currently named
`dealers-choice-server` (executable in `meson.build`, entry point
`src/server/server_main.c`, deprecation stub for the GUI's `--server` in
`src/cli.c`).  Andy isn't sold on the name — revisit before release.

Name proposals so far:
- `dealers-choice-server` (current placeholder)
- `dealers-choice-listen-for-clients-who-wish-to-player-poker-and-accept-the-connection-if-there-are-no-errors-while-connecting`
  (Andy's, submitted in jest — kept here so the bar for "too verbose" stays well documented)

Whatever it lands on, these all reference it and must change together: `meson.build`,
the GUI `--server` deprecation message (`src/cli.c`), `tests/game_logic.py`,
`scripts/_harness_common.sh`, `docker/docker-compose.yml`, and the docs
(`README.md`, `docs/CONFIG.md`, `tests/README.md`).
