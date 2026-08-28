# Notes for AI coding agents

Working notes for any agent or tool working in this repository, kept
tool-neutral so they are not specific to one assistant.

## Where things are

- **`CLAUDE.md`** (repo root) — the project map: layout, build structure,
  networking and protocol notes, conventions, and the rules that override
  default behaviour. Read it first whatever tool you are. It is named for
  historical reasons; nothing in it is Claude-specific.
- **`.claude/skills/`** — task-specific guides, one directory per topic, each a
  `SKILL.md` with a `name` and a `description` saying when it applies. Read the
  description to decide whether a guide is relevant before reading the body.
  The directory is named for the tool that discovers them automatically, but the
  guides are plain Markdown and are meant for any agent working here.

Current guides:

| Guide | Read it when |
| --- | --- |
| `dc-testing` | Running or probing live binaries, reproducing a networking bug, or a fix appears not to work |
| `tcpme` | Editing or auditing `src/tcpme/`, the socket layer, or any wire parser |
| `pokeval` | Working on hand evaluation in `src/pokeval/` |

## Build and test

The project uses meson. Configure a build directory, then:

```sh
meson setup <builddir>
meson compile -C <builddir>
meson test -C <builddir>
```

Several build directories usually coexist (plain, ASan, TSan), so no single name
is canonical — list them with `ls -d */meson-info | xargs -n1 dirname`. New C
code should be verified under a sanitizer build, not only a plain one.

Build details, including the static-library layout and the optional
`-Dregistry=true`, are in `CLAUDE.md` and `BUILD.md`.

## House rules worth knowing up front

- **Do not change `GAME_PROTOCOL_VERSION`.** The maintainer owns that bump.
- **The networking and server code was largely written by an LLM** and has not
  had an independent security review. Treat anything reachable from a socket as
  needing more scrutiny than hand-written code, and do not claim it is safe.
- **Match the surrounding code style** rather than reformatting. Formatting runs
  (`clang-format`, `meson fmt`) happen separately, on their own commits.
- Comments are welcome in this project, including ones written to orient a
  future agent session.

## Keeping these notes current

The guides under `.claude/skills/` are meant to be corrected and extended by
whoever notices they are wrong — each carries its own instructions for that. A
guide describing a flag, path or behaviour that no longer exists is worse than no
guide, because it will be believed. Fix it in place when you find it stale.
