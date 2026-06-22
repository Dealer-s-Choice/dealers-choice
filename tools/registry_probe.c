/*
 registry_probe.c
 https://github.com/Dealer-s-Choice/dealers-choice

 MIT License

 Copyright (c) 2026 Andy Alt

 Initial version written by Claude (Opus 4.8, an LLM by Anthropic) at Andy's
 direction.
*/

/*
  Manual test tool for the server registry (not installed). It can announce a
  fake server to a registry and request the list, so the daemon can be exercised
  without the full game server/client integration.

    registry_probe announce <reg_host> <reg_port> <game_port> [name]
    registry_probe list     <reg_host> <reg_port>
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "net.h"
#include "registry.h"

int main(int argc, char *argv[]) {
  if (argc < 4) {
    fprintf(stderr,
            "usage:\n"
            "  %s announce <reg_host> <reg_port> <game_port> [name]\n"
            "  %s list     <reg_host> <reg_port>\n",
            argv[0], argv[0]);
    return 1;
  }
  if (tcpme_init() != 0) {
    fputs("tcpme_init failed\n", stderr);
    return 1;
  }

  const char *host = argv[2];
  uint16_t reg_port = (uint16_t)atoi(argv[3]);
  int rc = 1;

  if (strcmp(argv[1], "announce") == 0 && argc >= 5) {
    tcpme_socket_t s = tcpme_connect(host, reg_port);
    if (!tcpme_socket_valid(s)) {
      fprintf(stderr, "connect to registry %s:%u failed\n", host, reg_port);
    } else {
      RegistryServer_t srv = {0};
      srv.tcp_port = (uint16_t)atoi(argv[4]);
      srv.player_count = 2;
      srv.max_players = 5;
      srv.password_protected = true;
      srv.in_progress = false;
      snprintf(srv.name, sizeof srv.name, "%s", argc >= 6 ? argv[5] : "Probe Server");
      rc = registry_send_announce(s, &srv);
      printf("announce sent (rc=%d) game_port=%u name=\"%s\"\n", rc, srv.tcp_port, srv.name);
      tcpme_close(s);
    }
  } else if (strcmp(argv[1], "list") == 0) {
    tcpme_socket_t s = tcpme_connect(host, reg_port);
    if (!tcpme_socket_valid(s)) {
      fprintf(stderr, "connect to registry %s:%u failed\n", host, reg_port);
    } else {
      if (registry_send_list_request(s) == 0) {
        RegistryServer_t out[REGISTRY_MAX_SERVERS];
        int n = 0;
        if (registry_recv_list(s, out, REGISTRY_MAX_SERVERS, &n) == 0) {
          printf("%d server(s):\n", n);
          for (int i = 0; i < n; i++)
            printf("  %s:%u  \"%s\"  %u/%u  %s%s\n", out[i].ip, out[i].tcp_port, out[i].name,
                   out[i].player_count, out[i].max_players,
                   out[i].password_protected ? "[pw]" : "", out[i].in_progress ? "[playing]" : "");
          rc = 0;
        } else {
          fputs("failed to receive list\n", stderr);
        }
      }
      tcpme_close(s);
    }
  } else {
    fprintf(stderr, "unknown command: %s\n", argv[1]);
  }

  tcpme_quit();
  return rc;
}
