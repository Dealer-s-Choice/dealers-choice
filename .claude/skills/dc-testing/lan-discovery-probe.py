#!/usr/bin/env python3
"""Probe Dealer's Choice LAN discovery and report each server's instance_id.

Sends real discovery queries over IPv4 broadcast and loopback, prints every
reply (source address, advertised TCP port, instance_id), and counts distinct
ids at the end. Needs no client build and no display.

    python3 .claude/skills/dc-testing/lan-discovery-probe.py [discovery_port]

Passing condition: distinct ids == the number of server processes running.
Several source addresses sharing one id in a round is correct (one server
answers per interface). Ids that change between rounds mean the server is
rerolling its identity -- see issue #368.

The wire format is documented at the top of src/net/lan_discovery.c and is
hardcoded below; a format change needs the same edit here.
"""
import socket, struct, sys, time
PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 22787
MAGIC = b"DCLAN"
q = MAGIC + b"Q" + bytes([1])
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
s.settimeout(1.0)
seen = []
for round_no in range(5):
    s.sendto(q, ("255.255.255.255", PORT))
    s.sendto(q, ("127.0.0.1", PORT))
    t0 = time.time()
    while time.time() - t0 < 0.8:
        try:
            data, addr = s.recvfrom(512)
        except socket.timeout:
            break
        if len(data) >= 16 and data[:5] == MAGIC and data[5:6] == b"R":
            port, pc, mp, flags = struct.unpack(">HBBB", data[7:12])
            inst = struct.unpack(">I", data[12:16])[0]
            seen.append(inst)
            print(f"round {round_no} from {addr[0]:15s} tcp_port={port} instance_id=0x{inst:08x}")
    time.sleep(0.5)
print(f"\ndistinct instance_ids seen: {len(set(seen))} over {len(seen)} replies")
