/*
 lan_discovery.c
 https://github.com/Dealer-s-Choice/dealers-choice

 MIT License

 Copyright (c) 2026 Andy Alt
*/

#include "lan_discovery.h"

#include <string.h>

/* Discovery wire protocol, versioned independently of GAME_PROTOCOL_VERSION.
 * Query:    magic[5] 'Q' ver
 * Response: magic[5] 'R' ver port_hi port_lo player_count max_players flags
 *           name_len name[name_len]
 * All multi-byte fields are big-endian. */
#define LAN_MAGIC "DCLAN"
#define LAN_MAGIC_LEN 5
#define LAN_DISC_VERSION 1
#define LAN_MSG_QUERY 'Q'
#define LAN_MSG_RESPONSE 'R'

#define LAN_FLAG_PASSWORD 0x01
#define LAN_FLAG_IN_PROGRESS 0x02

#define LAN_HDR_LEN (LAN_MAGIC_LEN + 2)              /* magic + type + version */
#define LAN_RESP_FIXED_LEN (LAN_HDR_LEN + 2 + 1 + 1 + 1 + 1) /* + port,pc,mp,flags,namelen */
#define LAN_PKT_MAX (LAN_RESP_FIXED_LEN + LAN_NAME_MAX)

static bool valid_header(const unsigned char *p, int n, char want_type) {
  return n >= LAN_HDR_LEN && memcmp(p, LAN_MAGIC, LAN_MAGIC_LEN) == 0 &&
         p[LAN_MAGIC_LEN] == (unsigned char)want_type && p[LAN_MAGIC_LEN + 1] == LAN_DISC_VERSION;
}

/* Length of a NUL-terminated name, capped at LAN_NAME_MAX (no strnlen: not
 * portable to all DC targets). */
static size_t name_length(const char *name) {
  size_t i = 0;
  while (i < LAN_NAME_MAX && name[i] != '\0')
    i++;
  return i;
}

tcpme_socket_t lan_discovery_open_responder(void) { return tcpme_udp_open(LAN_DISCOVERY_PORT, false); }

bool lan_discovery_answer(tcpme_socket_t sock, const LanGameInfo_t *info) {
  unsigned char buf[LAN_PKT_MAX];
  char from_ip[TCPME_ADDRSTRLEN];
  uint16_t from_port = 0;
  int n = tcpme_udp_recvfrom(sock, buf, sizeof(buf), from_ip, sizeof(from_ip), &from_port);
  if (n < 0 || !valid_header(buf, n, LAN_MSG_QUERY))
    return false; /* ignore stray/foreign datagrams */

  size_t name_len = name_length(info->name);
  unsigned char out[LAN_PKT_MAX];
  size_t o = 0;
  memcpy(out + o, LAN_MAGIC, LAN_MAGIC_LEN);
  o += LAN_MAGIC_LEN;
  out[o++] = LAN_MSG_RESPONSE;
  out[o++] = LAN_DISC_VERSION;
  out[o++] = (unsigned char)(info->tcp_port >> 8);
  out[o++] = (unsigned char)(info->tcp_port & 0xff);
  out[o++] = info->player_count;
  out[o++] = info->max_players;
  out[o++] = (unsigned char)((info->password_protected ? LAN_FLAG_PASSWORD : 0) |
                             (info->in_progress ? LAN_FLAG_IN_PROGRESS : 0));
  out[o++] = (unsigned char)name_len;
  memcpy(out + o, info->name, name_len);
  o += name_len;

  return tcpme_udp_sendto(sock, from_ip, from_port, out, (int)o) == (int)o;
}

tcpme_socket_t lan_discovery_open_client(void) { return tcpme_udp_open(0, true); }

bool lan_discovery_query(tcpme_socket_t sock) {
  unsigned char q[LAN_HDR_LEN];
  memcpy(q, LAN_MAGIC, LAN_MAGIC_LEN);
  q[LAN_MAGIC_LEN] = LAN_MSG_QUERY;
  q[LAN_MAGIC_LEN + 1] = LAN_DISC_VERSION;

  bool sent_bcast =
      tcpme_udp_broadcast(sock, LAN_DISCOVERY_PORT, q, (int)sizeof(q)) == (int)sizeof(q);

  /* Also query loopback directly: limited broadcast (255.255.255.255) is not
   * reliably looped back to a responder on the same host, so without this a
   * server and client on one machine (the common test setup) wouldn't find
   * each other. */
  bool sent_loop =
      tcpme_udp_sendto(sock, "127.0.0.1", LAN_DISCOVERY_PORT, q, (int)sizeof(q)) == (int)sizeof(q);

  /* Report success if either query went out. Some hosts have no
   * broadcast-capable route (restricted CI runners, loopback-only setups),
   * where loopback discovery still works and must not count as a failure. */
  return sent_bcast || sent_loop;
}

bool lan_discovery_read_response(tcpme_socket_t sock, LanGameInfo_t *out) {
  unsigned char buf[LAN_PKT_MAX];
  char from_ip[TCPME_ADDRSTRLEN];
  uint16_t from_port = 0;
  int n = tcpme_udp_recvfrom(sock, buf, sizeof(buf), from_ip, sizeof(from_ip), &from_port);
  if (n < LAN_RESP_FIXED_LEN || !valid_header(buf, n, LAN_MSG_RESPONSE))
    return false;

  memset(out, 0, sizeof(*out));
  size_t o = LAN_HDR_LEN;
  out->tcp_port = (uint16_t)((buf[o] << 8) | buf[o + 1]);
  o += 2;
  out->player_count = buf[o++];
  out->max_players = buf[o++];
  unsigned char flags = buf[o++];
  out->password_protected = (flags & LAN_FLAG_PASSWORD) != 0;
  out->in_progress = (flags & LAN_FLAG_IN_PROGRESS) != 0;
  unsigned char name_len = buf[o++];
  if (name_len > LAN_NAME_MAX)
    name_len = LAN_NAME_MAX;
  if ((size_t)n - o < name_len)
    name_len = (unsigned char)((size_t)n - o); /* truncated datagram guard */
  memcpy(out->name, buf + o, name_len);
  out->name[name_len] = '\0';

  strncpy(out->ip, from_ip, sizeof(out->ip) - 1);
  out->ip[sizeof(out->ip) - 1] = '\0';
  return true;
}
