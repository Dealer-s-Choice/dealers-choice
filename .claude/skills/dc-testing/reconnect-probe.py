#!/usr/bin/env python3
"""
#112 reconnect-with-stack probe: does a dropped player get their seat back?

Speaks the join handshake directly. Against a DC_TEST=1 server the password and
nick steps are skipped on both ends, so a join is just:

    -> GameProtocolHeader_t (11 bytes)      <- 1-byte ACK
    -> 32-byte reconnect claim (zeros = none)
    <- 32-byte issued session token

Sequence: join, drop, rejoin presenting the issued token, and check the server
logged the reclaim. Also checks that a garbage token falls through to a normal
join rather than being refused.

Usage (server must be running with DC_TEST=1):
    python3 reconnect-probe.py [--port 22999] [--log /tmp/dc_reconnect_server.log]
"""
import argparse, os, socket, struct, sys, time

MAGIC = b"DCPROTO\0"
VERSION = 11                    # GAME_PROTOCOL_VERSION (src/net/net.h)
TOKEN_LEN = 32                  # RECONNECT_TOKEN_LEN (src/types.h)
MAX_CLIENTS = 5                 # MAX_CLIENTS (src/types.h)

def header(flags=0):
    return MAGIC + struct.pack(">H", VERSION) + struct.pack("B", flags)

def join(host, port, claim=None):
    """Complete a join; returns (socket, issued_token)."""
    s = socket.create_connection((host, port), timeout=10)
    s.sendall(header())
    ack = s.recv(1)
    if ack != b"\x00":
        raise SystemExit(f"server rejected the header: {ack!r}")
    s.sendall(claim if claim else b"\x00" * TOKEN_LEN)
    issued = b""
    while len(issued) < TOKEN_LEN:
        chunk = s.recv(TOKEN_LEN - len(issued))
        if not chunk:
            raise SystemExit("server closed before issuing a token")
        issued += chunk
    return s, issued

def case_eviction(a):
    """A hold reserves a seat, it does not owe it: with every seat taken or
    held, an arriving player must displace the longest-waiting hold rather than
    be turned away. Needs a FRESH server -- each earlier connection leaves a
    hold of its own, so the seats must start empty for the table to fill
    predictably."""
    seats = []
    for i in range(MAX_CLIENTS):
        s, _ = join(a.host, a.port)
        seats.append(s)
    print(f"filled all {MAX_CLIENTS} seats")

    seats[0].close()          # seat 0 becomes held; 1..4 still occupied
    time.sleep(1)

    # No free seat exists. The newcomer must still get in.
    late, _ = join(a.host, a.port)
    time.sleep(1)
    log = open(a.log).read() if os.path.exists(a.log) else ""
    evicted = "given up early" in log
    print(f"late joiner with a full table: {'displaced the hold' if evicted else 'NOT admitted'}")
    for s in seats[1:]:
        s.close()
    late.close()
    if not evicted:
        raise SystemExit("FAIL: a held seat kept a present player out")
    print("RESULT: PASS")
    return 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=22999)
    ap.add_argument("--log", default="/tmp/dc_reconnect_server.log")
    ap.add_argument("--case", choices=["reconnect", "eviction"], default="reconnect",
                    help="eviction needs a freshly started server")
    a = ap.parse_args()

    if a.case == "eviction":
        return case_eviction(a)

    s1, tok = join(a.host, a.port)
    print(f"joined, issued token {tok.hex()[:16]}...")
    if tok == b"\x00" * TOKEN_LEN:
        raise SystemExit("FAIL: server issued an all-zero token")

    s1.close()                       # the drop
    time.sleep(1)

    # The seat must be RESERVED, not merely restorable: a different client
    # joining during the grace window must not be handed seat 0.
    before = open(a.log).read() if os.path.exists(a.log) else ""
    other, _ = join(a.host, a.port)
    time.sleep(1)
    new_lines = (open(a.log).read() if os.path.exists(a.log) else "")[len(before):]
    took_zero = "slot 0 taken" in new_lines
    print(f"other client during hold: {'TOOK SEAT 0' if took_zero else 'got a different seat'}")
    if took_zero:
        raise SystemExit("FAIL: held seat was handed to another client")
    other.close()
    time.sleep(1)

    s2, tok2 = join(a.host, a.port, claim=tok)
    print(f"rejoined, new token {tok2.hex()[:16]}...")
    if tok2 == tok:
        raise SystemExit("FAIL: token was reused, not re-minted (replayable)")
    s2.close()
    time.sleep(1)

    # A token the server has never seen must not be refused.
    s3, _ = join(a.host, a.port, claim=b"\xAB" * TOKEN_LEN)
    print("unknown token: joined normally (did not fail closed)")
    s3.close()
    time.sleep(1)

    log = open(a.log).read() if os.path.exists(a.log) else ""
    reclaimed = log.count("reclaimed by")
    print(f"server logged {reclaimed} reclaim(s)")
    ok = reclaimed >= 1
    print("RESULT:", "PASS" if ok else "FAIL")
    return 0 if ok else 1

if __name__ == "__main__":
    sys.exit(main())
