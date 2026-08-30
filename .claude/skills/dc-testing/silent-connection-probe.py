#!/usr/bin/env python3
"""
#363/#119 regression probe: does a silent TCP connection freeze the server loop?

Pre-fix, an accepted socket went straight into a blocking recv, so a peer that
connected and said nothing wedged the single-threaded loop for
SOCKET_IO_TIMEOUT_MS (30s). Post-fix it is parked in the pending set and reaped
at HANDSHAKE_DEADLINE_MS (5s) without ever blocking.

Measures how long a legitimate PROTO_FLAG_PROBE handshake takes with and
without N silent connections held open. A pass keeps the probe in the
milliseconds; a regression pushes it toward 30s.

Usage (server must already be listening):
    python3 silent_conn_test.py [--host 127.0.0.1] [--port 22999] [--silent 3]
"""
import argparse, socket, struct, sys, time

MAGIC = b"DCPROTO\0"          # char magic[sizeof("DCPROTO")] -> 8 bytes
VERSION = 11                   # GAME_PROTOCOL_VERSION (net.h)
PROTO_FLAG_PROBE = 0x02

def header(flags):
    return MAGIC + struct.pack(">H", VERSION) + struct.pack("B", flags)

def probe(host, port, timeout=35.0):
    """Full probe handshake; returns seconds until the server's ACK byte."""
    start = time.monotonic()
    s = socket.create_connection((host, port), timeout=timeout)
    try:
        s.sendall(header(PROTO_FLAG_PROBE))
        ack = s.recv(1)
    finally:
        s.close()
    if ack != b"\x00":
        raise SystemExit(f"unexpected ACK {ack!r} (expected b'\\x00')")
    return time.monotonic() - start

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=22999)
    ap.add_argument("--silent", type=int, default=3)
    a = ap.parse_args()

    base = probe(a.host, a.port)
    print(f"baseline probe:              {base*1000:8.1f} ms")

    held = []
    for _ in range(a.silent):
        c = socket.create_connection((a.host, a.port), timeout=5)
        held.append(c)          # connected, sends nothing, stays open
    print(f"opened {a.silent} silent connection(s)")

    t = probe(a.host, a.port)
    print(f"probe with silent held:      {t*1000:8.1f} ms")

    # The reaper should close them around HANDSHAKE_DEADLINE_MS (5s).
    time.sleep(7)
    reaped = sum(1 for c in held if not c.recv(1))
    print(f"silent connections reaped:   {reaped}/{a.silent} after 7s")
    for c in held:
        c.close()

    ok = t < 2.0 and reaped == a.silent
    print("RESULT:", "PASS" if ok else "FAIL")
    return 0 if ok else 1

if __name__ == "__main__":
    sys.exit(main())
