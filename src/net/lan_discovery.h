/*
 lan_discovery.h
 https://github.com/Dealer-s-Choice/dealers-choice

 MIT License

 Copyright (c) 2026 Andy Alt
*/

/*
  LAN game discovery: a host answers UDP broadcast queries so clients on the
  same network can find open games without a central/registry server. This is
  DC policy (the discovery port and packet format) layered over tcpme's generic
  UDP primitives. The discovery protocol is versioned independently of
  GAME_PROTOCOL_VERSION.
*/

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "tcpme/tcpme.h"

/* Default UDP port for LAN discovery, used when common.conf does not set
 * lan_discovery_port. Distinct from the game's TCP port (22777) and the test
 * harness base port (22778). The actual port is read from config (it must match
 * across all machines on the LAN), so call sites pass it in rather than using
 * this constant directly. */
#define LAN_DISCOVERY_PORT 22787

/* Link-local (ff02::/16) IPv6 multicast group for discovery, the IPv6 counterpart
 * to IPv4's limited broadcast. Client and server must agree on it; it pairs with
 * LAN_DISCOVERY_PORT. (4443 = 'D''C' in hex.) */
#define LAN_DISCOVERY_MCAST6 "ff02::4443"

/* Max length (excluding the terminating NUL) of an advertised server name. */
#define LAN_NAME_MAX 32

/* A game advertised by a host / discovered by a client. */
typedef struct {
  /* Set by the client from the reply source. Sized for an IPv6 literal plus a
   * "%<scope>" suffix (link-local addresses need the interface scope to stay
   * connectable), so a little larger than a bare TCPME_ADDRSTRLEN. */
  char ip[TCPME_ADDRSTRLEN + 12];
  uint16_t tcp_port;           /* TCP port to connect to */
  uint8_t player_count;        /* players currently seated */
  uint8_t max_players;         /* seat capacity */
  bool password_protected;
  bool in_progress;
  uint32_t instance_id;        /* random per-server id; client dedups multi-NIC replies on it */
  char name[LAN_NAME_MAX + 1]; /* optional; may be empty */
} LanGameInfo_t;

/* --- Host (responder) side --- */

/* Open the UDP responder socket bound to `port`. Add it to a select set; when
 * readable, call lan_discovery_answer(). Returns TCPME_INVALID_SOCKET on failure
 * (LAN discovery may be treated as optional). */
tcpme_socket_t lan_discovery_open_responder(uint16_t port);

/* If a valid discovery query is pending on sock, unicast a response built from
 * *info back to the querier. Returns true if a query was handled. */
bool lan_discovery_answer(tcpme_socket_t sock, const LanGameInfo_t *info);

/* IPv6 counterparts. The responder binds [::]:port and joins LAN_DISCOVERY_MCAST6
 * (it must be added to the same select set; service it with answer6). The IPv6
 * group is fixed (LAN_DISCOVERY_MCAST6), but it is reached on LAN_DISCOVERY_PORT,
 * so a configured discovery port still applies. Returns TCPME_INVALID_SOCKET on
 * failure (IPv6 discovery is optional, like the IPv4 path). */
tcpme_socket_t lan_discovery_open_responder6(uint16_t port);
bool lan_discovery_answer6(tcpme_socket_t sock, const LanGameInfo_t *info);

/* --- Client (browser) side --- */

/* Open a broadcast-capable, ephemeral discovery socket. Returns
 * TCPME_INVALID_SOCKET on failure. */
tcpme_socket_t lan_discovery_open_client(void);

/* Broadcast a discovery query to `port`. Returns true on success. */
bool lan_discovery_query(tcpme_socket_t sock, uint16_t port);

/* Parse one pending response on sock into *out (out->ip is set from the reply's
 * source address). Returns true if a valid response was read. */
bool lan_discovery_read_response(tcpme_socket_t sock, LanGameInfo_t *out);

/* IPv6 counterparts. query6 multicasts to LAN_DISCOVERY_MCAST6:LAN_DISCOVERY_PORT
 * on every interface; read_response6 fills out->ip from the reply source,
 * appending "%<scope>" for a link-local (fe80::) address so it stays connectable.
 * Open the client socket with open_client6. */
tcpme_socket_t lan_discovery_open_client6(void);
bool lan_discovery_query6(tcpme_socket_t sock);
bool lan_discovery_read_response6(tcpme_socket_t sock, LanGameInfo_t *out);
