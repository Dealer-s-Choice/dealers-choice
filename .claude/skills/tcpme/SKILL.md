---
name: tcpme
description: How to work on Dealer's Choice's `tcpme` cross-platform socket library (src/tcpme/) — its design contract (mechanism-not-policy, keep it DC-agnostic), the POSIX↔Winsock portability traps it handles, and a security-audit checklist for the code (it is LLM-written and internet-facing). Use when editing or reviewing anything in `src/tcpme/`, DC's networking code (net.c framing, server/registry/LAN-discovery), adding a socket option / IPv6 / UDP path, or auditing the socket and wire-parser code.
---

# tcpme — DC's cross-platform socket library

`src/tcpme/` is a small, SDL_net-inspired TCP/UDP socket abstraction over POSIX
sockets + Winsock (`tcpme.c` ~890 lines, `tcpme.h` ~250, plus a test suite under
`src/tcpme/tests/`). It's `tcpme_dep`, merged into `shared_dep`, linked by all
three binaries. **It was written largely by an LLM — treat it as needing more
scrutiny than hand-written code, especially anything reachable from the network.**

## The contract (this is a policy, follow it)

**Keep tcpme generic and DC-agnostic.** It's meant to be a standalone, reusable
library (SDL_net-style): it provides **mechanism** only; DC provides **policy**.
Do NOT put DC-specific values, constants, comments, or assumptions in
`src/tcpme/`. When a fix needs a tunable or a project decision, **expose a knob in
tcpme and set it from DC** — don't hardcode it in the library.

- Example: socket I/O timeouts. tcpme exposes `tcpme_set_timeout(sock, ms)` (the
  primitive); DC owns the value (`SOCKET_IO_TIMEOUT_MS` in `net.h`) and applies it
  after connect/accept. That split is the model for every new tunable.

## Use the abstraction, not raw calls

Everything goes through the tcpme types/API so the POSIX/Winsock split stays in
one place:

- **Type/handle:** `tcpme_socket_t` (`SOCKET` on Windows, `int` on POSIX),
  `TCPME_INVALID_SOCKET`, `tcpme_socket_valid(s)`. Never compare a socket to `-1`
  or `0` directly.
- **Lifecycle:** `tcpme_init()` (WSAStartup on Win, no-op else) before anything;
  `tcpme_quit()` at the end.
- **Errors:** `tcpme_get_error()` (thread-local last-error string). Never read raw
  `errno` / `WSAGetLastError()` in DC — tcpme bridges them.
- **TCP:** `tcpme_listen` / `tcpme_accept` (non-blocking; INVALID ≠ error, means
  "nothing pending") / `tcpme_connect[_timeout][_pref4]` / `tcpme_close` /
  `tcpme_send` / `tcpme_recv` / `tcpme_set_timeout`.
- **Readiness:** the `tcpme_set_t` poll wrapper (poll()/WSAPoll, so no
  FD_SETSIZE ceiling; the timed-connect path still uses select() because
  WSAPoll cannot report a failed non-blocking connect) — `tcpme_alloc_set` /
  `add`/`del_socket` / `tcpme_check_sockets(set, ms)` / `tcpme_socket_ready`.
- **UDP discovery:** IPv4 (`tcpme_udp_open`/`broadcast`/`sendto`/`recvfrom`) and
  IPv6 multicast (`tcpme_udp_open6` V6ONLY / `mcast6_join_all` /
  `udp_mcast6_send_all` / `recvfrom6` with a scope id for link-local).
- **Framing helpers:** `tcpme_put_be16/32` (pure-shift, endianness-safe) — use
  these for protocol headers, never a raw cast/`htons` of a struct.
- **Addresses:** `tcpme_get_peer_addr`/`local_addr`/`peer_ip` (buffers sized with
  `TCPME_ADDRSTRLEN` / `TCPME_ADDRPORTSTRLEN`).

## POSIX ↔ Winsock traps tcpme handles (and your edits must too)

- `close()` vs `closesocket()`; `WSAStartup`/`WSACleanup` vs nothing.
- Errors: `errno` vs `WSAGetLastError()` (different codes, e.g. `EWOULDBLOCK` vs
  `WSAEWOULDBLOCK`); go through `set_error`/`tcpme_get_error`.
- **Return widths:** `send`/`recv` return `ssize_t` on POSIX but `int` on Winsock,
  and take an `int` length. Passing a `size_t > INT_MAX` truncates to a negative
  int — clamp to `INT_MAX` before the call (this is exactly CID 647154 in
  `send_all_tcp`).
- `MSG_NOSIGNAL` doesn't exist on Windows/macOS — suppress SIGPIPE another way
  (`SO_NOSIGPIPE` / ignore the signal); don't assume the flag.
- Dual-stack: an IPv6 socket must set `IPV6_V6ONLY` so a v4 and a v6 socket can
  share a port (the LAN-discovery pattern); link-local multicast needs the
  interface **scope id**.
- `EINTR`/`EAGAIN` retry semantics differ; loop, don't treat as fatal.

## Audit checklist (LLM-written + internet-facing → scrutinize)

When touching or reviewing this code, actively check:

- **Partial send/recv loops** — `send`/`recv` can transfer fewer bytes than asked;
  every send/recv must loop until complete or error (see `send_all_tcp` /
  `recv_all_tcp` in `net.c`, moving into tcpme per BACKLOG #54).
- **`size_t`→`int` truncation** at the socket boundary (above).
- **Unchecked `setsockopt`/`bind`/`listen` returns** — Coverity keeps flagging
  these (the CHECKED_RETURN cluster). A failed `setsockopt` should not be ignored.
- **Untrusted framing.** The DCPROTO magic+version+length parser and the UDP
  discovery parsers read attacker-controlled bytes: validate length before
  allocating/reading, cap sizes, don't trust a peer-supplied count. This is the
  area behind the #299/#41 recv-framing + slot_taken desync and what the
  `tests/net_fuzz.c` / `tests/registry_fuzz.c` fuzzers target — run them.
- **Timeouts** so a slow/unreachable/malicious peer can't hang a thread
  (`tcpme_connect_timeout`, `SO_RCVTIMEO`).

## Testing

- Unit tests live in `src/tcpme/tests/` (`test_timeout`, `test_ipv6`, `test_udp`,
  `test_protocol`, `test_peer_close`, `test_error_paths`, `test_socket_set`, …) —
  add one alongside any new behavior.
- Build the wire parsers under **ASan/UBSan** and run the fuzzers; TSan for the
  select/accept-loop concurrency.

## Future portability ideas (from prior notes; not yet in code)

A `SOCKERRNO` macro to unify `errno`/`WSAGetLastError`, feature-detected includes,
and `sread`/`swrite` cast macros for the `ssize_t`↔`int` boundary — lower-priority
cleanups modeled on curl's portability layer.
