/*
 registry.c
 https://github.com/Dealer-s-Choice/dealers-choice

 MIT License

 Copyright (c) 2026 Andy Alt

 Initial version written by Claude (Opus 4.8, an LLM by Anthropic) at Andy's
 direction.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "net.h"
#include "registry.h"

/* Strip control characters and cap length; result is NUL-terminated. Used on
 * both ends so a hostile name can't inject control bytes into logs/the JSON. */
static void sanitize_name(const char *src, char *dst, size_t dstsz) {
  size_t j = 0;
  if (src) {
    for (size_t i = 0; src[i] && j + 1 < dstsz; i++) {
      unsigned char c = (unsigned char)src[i];
      if (c >= 0x20 && c != 0x7f)
        dst[j++] = (char)c;
    }
  }
  dst[j] = '\0';
}

uint8_t *registry_recv_frame(tcpme_socket_t sock, uint16_t *opcode, const uint8_t **payload,
                            size_t *payload_len) {
  uint32_t size_net = 0;
  if (recv_all_tcp(sock, &size_net, sizeof(size_net)) <= 0)
    return NULL;
  uint32_t size = tcpme_get_be32((const uint8_t *)&size_net);
  if (size < OPCODE_SIZE || size > 65536)
    return NULL;
  uint8_t *buf = malloc(size);
  if (!buf)
    return NULL;
  if (recv_all_tcp(sock, buf, size) <= 0) {
    free(buf);
    return NULL;
  }
  *opcode = tcpme_get_be16(buf);
  *payload = buf + OPCODE_SIZE;
  *payload_len = size - OPCODE_SIZE;
  return buf;
}

int registry_send_announce(tcpme_socket_t sock, const RegistryServer_t *info) {
  ServerAnnounce msg = SERVER_ANNOUNCE__INIT;
  msg.registry_version = REGISTRY_PROTOCOL_VERSION;
  msg.name = (char *)info->name;
  msg.tcp_port = info->tcp_port;
  msg.player_count = info->player_count;
  msg.max_players = info->max_players;
  msg.password_protected = info->password_protected;
  msg.in_progress = info->in_progress;

  size_t len = server_announce__get_packed_size(&msg);
  uint8_t *buf = malloc(len);
  if (!buf)
    return -1;
  server_announce__pack(&msg, buf);
  int rc = send_message(sock, MSG_REG_ANNOUNCE, buf, len);
  free(buf);
  return rc;
}

int registry_send_list_request(tcpme_socket_t sock) {
  ServerListRequest msg = SERVER_LIST_REQUEST__INIT;
  msg.registry_version = REGISTRY_PROTOCOL_VERSION;
  uint8_t buf[16];
  size_t len = server_list_request__get_packed_size(&msg);
  server_list_request__pack(&msg, buf);
  return send_message(sock, MSG_REG_LIST_REQUEST, buf, len);
}

int registry_parse_announce(const uint8_t *payload, size_t len, RegistryServer_t *out) {
  ServerAnnounce *msg = server_announce__unpack(NULL, len, payload);
  if (!msg)
    return -1;
  if (msg->registry_version != REGISTRY_PROTOCOL_VERSION) {
    server_announce__free_unpacked(msg, NULL);
    return -1;
  }
  memset(out, 0, sizeof(*out));
  out->tcp_port = (uint16_t)msg->tcp_port;
  out->player_count = (uint8_t)msg->player_count;
  out->max_players = (uint8_t)msg->max_players;
  out->password_protected = msg->password_protected;
  out->in_progress = msg->in_progress;
  sanitize_name(msg->name, out->name, sizeof(out->name));
  server_announce__free_unpacked(msg, NULL);
  return 0;
}

int registry_send_list(tcpme_socket_t sock, const RegistryServer_t *servers, int count) {
  if (count < 0)
    count = 0;
  if (count > REGISTRY_MAX_SERVERS)
    count = REGISTRY_MAX_SERVERS;

  ServerList list = SERVER_LIST__INIT;
  ServerEntry *entries = count ? calloc((size_t)count, sizeof(*entries)) : NULL;
  ServerEntry **ptrs = count ? calloc((size_t)count, sizeof(*ptrs)) : NULL;
  if (count && (!entries || !ptrs)) {
    free(entries);
    free(ptrs);
    return -1;
  }
  for (int i = 0; i < count; i++) {
    server_entry__init(&entries[i]);
    entries[i].ip = (char *)servers[i].ip;
    entries[i].tcp_port = servers[i].tcp_port;
    entries[i].name = (char *)servers[i].name;
    entries[i].player_count = servers[i].player_count;
    entries[i].max_players = servers[i].max_players;
    entries[i].password_protected = servers[i].password_protected;
    entries[i].in_progress = servers[i].in_progress;
    ptrs[i] = &entries[i];
  }
  list.n_servers = (size_t)count;
  list.servers = ptrs;

  size_t len = server_list__get_packed_size(&list);
  uint8_t *buf = malloc(len ? len : 1);
  int rc = -1;
  if (buf) {
    server_list__pack(&list, buf);
    rc = send_message(sock, MSG_REG_LIST, buf, len);
    free(buf);
  }
  free(entries);
  free(ptrs);
  return rc;
}

int registry_recv_list(tcpme_socket_t sock, RegistryServer_t *out, int max, int *count) {
  *count = 0;
  uint16_t opcode = 0;
  const uint8_t *payload = NULL;
  size_t payload_len = 0;
  uint8_t *frame = registry_recv_frame(sock, &opcode, &payload, &payload_len);
  if (!frame)
    return -1;
  if (opcode != MSG_REG_LIST) {
    free(frame);
    return -1;
  }
  ServerList *list = server_list__unpack(NULL, payload_len, payload);
  free(frame);
  if (!list)
    return -1;
  int n = 0;
  for (size_t i = 0; i < list->n_servers && n < max; i++) {
    ServerEntry *e = list->servers[i];
    RegistryServer_t *d = &out[n++];
    memset(d, 0, sizeof(*d));
    if (e->ip)
      snprintf(d->ip, sizeof(d->ip), "%s", e->ip);
    d->tcp_port = (uint16_t)e->tcp_port;
    d->player_count = (uint8_t)e->player_count;
    d->max_players = (uint8_t)e->max_players;
    d->password_protected = e->password_protected;
    d->in_progress = e->in_progress;
    sanitize_name(e->name, d->name, sizeof(d->name));
  }
  *count = n;
  server_list__free_unpacked(list, NULL);
  return 0;
}
