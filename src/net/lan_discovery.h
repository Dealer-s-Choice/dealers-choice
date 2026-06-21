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

/* Max length (excluding the terminating NUL) of an advertised server name. */
#define LAN_NAME_MAX 32

/* A game advertised by a host / discovered by a client. */
typedef struct {
  char ip[TCPME_ADDRSTRLEN];   /* set by the client from the reply source */
  uint16_t tcp_port;           /* TCP port to connect to */
  uint8_t player_count;        /* players currently seated */
  uint8_t max_players;         /* seat capacity */
  bool password_protected;
  bool in_progress;
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

/* --- Client (browser) side --- */

/* Open a broadcast-capable, ephemeral discovery socket. Returns
 * TCPME_INVALID_SOCKET on failure. */
tcpme_socket_t lan_discovery_open_client(void);

/* Broadcast a discovery query to `port`. Returns true on success. */
bool lan_discovery_query(tcpme_socket_t sock, uint16_t port);

/* Parse one pending response on sock into *out (out->ip is set from the reply's
 * source address). Returns true if a valid response was read. */
bool lan_discovery_read_response(tcpme_socket_t sock, LanGameInfo_t *out);
