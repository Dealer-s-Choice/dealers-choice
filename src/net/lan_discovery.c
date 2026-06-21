/*
 lan_discovery.c
 https://github.com/Dealer-s-Choice/dealers-choice

 MIT License

 Copyright (c) 2026 Andy Alt
*/

#include "lan_discovery.h"

#include <stdio.h>
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

/* Fill the fixed query datagram into q[LAN_HDR_LEN]. */
static void build_query(unsigned char *q) {
  memcpy(q, LAN_MAGIC, LAN_MAGIC_LEN);
  q[LAN_MAGIC_LEN] = LAN_MSG_QUERY;
  q[LAN_MAGIC_LEN + 1] = LAN_DISC_VERSION;
}

/* Serialize *info into out[LAN_PKT_MAX]; returns the datagram length. Shared by
 * the IPv4 and IPv6 responders so the wire format stays identical on both. */
static size_t build_response(unsigned char *out, const LanGameInfo_t *info) {
  size_t name_len = name_length(info->name);
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
  return o;
}

/* Parse a response datagram (n bytes) into *out, leaving out->ip for the caller
 * to fill from the packet source. Returns false on a malformed datagram. */
static bool parse_response(const unsigned char *buf, int n, LanGameInfo_t *out) {
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
  return true;
}

/* --- IPv4 (limited broadcast) --- */

tcpme_socket_t lan_discovery_open_responder(uint16_t port) { return tcpme_udp_open(port, false); }

bool lan_discovery_answer(tcpme_socket_t sock, const LanGameInfo_t *info) {
  unsigned char buf[LAN_PKT_MAX];
  char from_ip[TCPME_ADDRSTRLEN];
  uint16_t from_port = 0;
  int n = tcpme_udp_recvfrom(sock, buf, sizeof(buf), from_ip, sizeof(from_ip), &from_port);
  if (n < 0 || !valid_header(buf, n, LAN_MSG_QUERY))
    return false; /* ignore stray/foreign datagrams */

  unsigned char out[LAN_PKT_MAX];
  size_t o = build_response(out, info);
  return tcpme_udp_sendto(sock, from_ip, from_port, out, (int)o) == (int)o;
}

tcpme_socket_t lan_discovery_open_client(void) { return tcpme_udp_open(0, true); }

bool lan_discovery_query(tcpme_socket_t sock, uint16_t port) {
  unsigned char q[LAN_HDR_LEN];
  build_query(q);

  bool sent_bcast = tcpme_udp_broadcast(sock, port, q, (int)sizeof(q)) == (int)sizeof(q);

  /* Also query loopback directly: limited broadcast (255.255.255.255) is not
   * reliably looped back to a responder on the same host, so without this a
   * server and client on one machine (the common test setup) wouldn't find
   * each other. */
  bool sent_loop =
      tcpme_udp_sendto(sock, "127.0.0.1", port, q, (int)sizeof(q)) == (int)sizeof(q);

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
  if (!parse_response(buf, n, out))
    return false;
  strncpy(out->ip, from_ip, sizeof(out->ip) - 1);
  out->ip[sizeof(out->ip) - 1] = '\0';
  return true;
}

/* --- IPv6 (link-local multicast) ---
 *
 * IPv4 broadcast can't reach IPv6-only hosts and never yields an IPv6 address to
 * connect to. These mirror the IPv4 calls over the LAN_DISCOVERY_MCAST6 group.
 * The discovered address is the responder's source address; for a link-local
 * (fe80::) source the scope id (interface index) is appended as "%<scope>" so
 * the value is directly usable by getaddrinfo()/connect. Run both families in
 * parallel and dedup by ip:port (lan_upsert in the caller). */

tcpme_socket_t lan_discovery_open_responder6(uint16_t port) {
  tcpme_socket_t s = tcpme_udp_open6(port);
  if (tcpme_socket_valid(s))
    tcpme_mcast6_join_all(s, LAN_DISCOVERY_MCAST6); /* must join to receive queries */
  return s;
}

bool lan_discovery_answer6(tcpme_socket_t sock, const LanGameInfo_t *info) {
  unsigned char buf[LAN_PKT_MAX];
  char from_ip[TCPME_ADDRSTRLEN];
  unsigned from_scope = 0;
  uint16_t from_port = 0;
  int n = tcpme_udp_recvfrom6(sock, buf, sizeof(buf), from_ip, sizeof(from_ip), &from_scope,
                              &from_port);
  if (n < 0 || !valid_header(buf, n, LAN_MSG_QUERY))
    return false;

  unsigned char out[LAN_PKT_MAX];
  size_t o = build_response(out, info);
  return tcpme_udp_sendto6(sock, from_ip, from_scope, from_port, out, (int)o) == (int)o;
}

tcpme_socket_t lan_discovery_open_client6(void) { return tcpme_udp_open6(0); }

bool lan_discovery_query6(tcpme_socket_t sock) {
  unsigned char q[LAN_HDR_LEN];
  build_query(q);
  return tcpme_udp_mcast6_send_all(sock, LAN_DISCOVERY_MCAST6, LAN_DISCOVERY_PORT, q,
                                   (int)sizeof(q)) > 0;
}

bool lan_discovery_read_response6(tcpme_socket_t sock, LanGameInfo_t *out) {
  unsigned char buf[LAN_PKT_MAX];
  char from_ip[TCPME_ADDRSTRLEN];
  unsigned from_scope = 0;
  uint16_t from_port = 0;
  int n = tcpme_udp_recvfrom6(sock, buf, sizeof(buf), from_ip, sizeof(from_ip), &from_scope,
                              &from_port);
  if (!parse_response(buf, n, out))
    return false;

  /* A non-zero scope means a link-local source; keep the scope so the address
   * stays connectable (fe80:: alone is ambiguous across interfaces). */
  if (from_scope != 0)
    snprintf(out->ip, sizeof(out->ip), "%s%%%u", from_ip, from_scope);
  else
    snprintf(out->ip, sizeof(out->ip), "%s", from_ip);
  return true;
}
