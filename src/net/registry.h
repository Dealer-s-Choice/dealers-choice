/*
 registry.h
 https://github.com/Dealer-s-Choice/dealers-choice

 MIT License

 Copyright (c) 2026 Andy Alt
*/

/*
  Server registry (directory) protocol: a game server announces itself to a
  registry over TCP so clients can discover internet servers (the LAN-only
  counterpart is lan_discovery). DC policy layered over tcpme + protobuf-c; the
  registry protocol is versioned independently of GAME_PROTOCOL_VERSION.

  Trust model: the registry takes a server's IP from the announce connection's
  source address (never the payload) and callback-verifies the advertised
  host:port speaks the DC protocol before listing it. Clients treat the list as
  hints and still authenticate on connect.
*/

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "tcpme/tcpme.h"

#define REGISTRY_PROTOCOL_VERSION 1

/* Default TCP port for the registry. Well clear of the game port (22777), the
 * test base port (22778), and LAN discovery (22787). Operator-configurable. */
#define REGISTRY_DEFAULT_PORT "22070"

/* Max advertised server-name length (excluding the terminating NUL). */
#define REGISTRY_NAME_MAX 32

/* Hard cap on how many servers the registry holds / sends. */
#define REGISTRY_MAX_SERVERS 256

/* Registry wire opcodes. A protocol distinct from the game's EMsgOpcode_t,
 * carried over the same length-prefixed framing (net.c send_message + the
 * [size:4 BE][opcode:2 BE][payload] layout). Registry connections are separate
 * from game connections, so the opcode spaces never mix on one socket. */
typedef enum {
  MSG_REG_ANNOUNCE = 0x0001,     /* server -> registry: publish / heartbeat */
  MSG_REG_LIST_REQUEST = 0x0002, /* client -> registry: request the list */
  MSG_REG_LIST = 0x0003,         /* registry -> client: the current list */
} ERegistryOpcode_t;

/* A server as advertised, held by the registry, and shown to a client. */
typedef struct {
  char ip[TCPME_ADDRSTRLEN]; /* set by the registry from the announce source */
  uint16_t tcp_port;         /* game port to connect to */
  uint8_t player_count;
  uint8_t max_players;
  bool password_protected;
  bool in_progress;
  char name[REGISTRY_NAME_MAX + 1];
} RegistryServer_t;

/* Read one framed message ([size:4 BE][opcode:2 BE][payload]). Returns the
 * malloc'd buffer (caller frees) and sets *opcode, *payload (into the buffer,
 * just past the opcode) and *payload_len. NULL on error/disconnect. Shared by
 * the client list-recv and the registry's accept loop. */
uint8_t *registry_recv_frame(tcpme_socket_t sock, uint16_t *opcode, const uint8_t **payload,
                            size_t *payload_len);

/* --- Server (announcer) side --- */

/* Send one announce/heartbeat to a connected registry socket. The registry
 * ignores info->ip (it uses the connection source). Returns 0 on success. */
int registry_send_announce(tcpme_socket_t sock, const RegistryServer_t *info);

/* --- Client (browser) side --- */

/* Ask the registry for the current list. Returns 0 on success. */
int registry_send_list_request(tcpme_socket_t sock);

/* Receive a MSG_REG_LIST reply into out[0..max-1]; *count is set to the number
 * filled. Returns 0 on success, -1 on error/disconnect. */
int registry_recv_list(tcpme_socket_t sock, RegistryServer_t *out, int max, int *count);

/* --- Registry side --- */

/* Parse a received MSG_REG_ANNOUNCE payload (protobuf, excluding the opcode)
 * into *out, sanitizing the name (length cap, control chars stripped). The
 * caller fills out->ip from the connection source afterwards. Returns 0 on
 * success, -1 if the payload is malformed or the registry version mismatches. */
int registry_parse_announce(const uint8_t *payload, size_t len, RegistryServer_t *out);

/* Send the current list to a connected client socket. Returns 0 on success. */
int registry_send_list(tcpme_socket_t sock, const RegistryServer_t *servers, int count);
