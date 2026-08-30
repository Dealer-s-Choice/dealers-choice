/*
 * test_protocol.c — multi-client, multi-round PING/PONG protocol.
 *
 * A server thread accepts N_CLIENTS connections into a socket set that also
 * holds the listen socket. It services everything through tcpme_check_sockets,
 * transforming each "PING <id>:<round>" into "PONG <id>:<round>" and sending
 * it back. N_CLIENTS client threads each do N_ROUNDS exchanges then send QUIT.
 *
 * NOTE: the one-recv-per-message pattern is safe ONLY because the protocol is
 * lockstep — each client waits for its PONG before sending the next PING (and
 * QUIT goes out only after the last PONG), so at most one message is ever in
 * flight per connection and TCP cannot coalesce two of them into one recv.
 * If pipelining is ever added here, the reads must grow real framing.
 */

#include "tcpme_test_helpers.h"
#include <stdio.h>
#include <string.h>

#include "tcpme.h"

#define N_CLIENTS 3
#define N_ROUNDS 8

/* Both well under meson's 30s test timeout, so a stall ends as a failure that
 * says what stalled rather than as a SIGTERM that says nothing. */
#define SERVER_DEADLINE_MS 15000
#define CLIENT_IO_TIMEOUT_MS 5000

/* listen socket + one slot per client */
#define SET_CAP (N_CLIENTS + 1)

/* ------------------------------------------------------------------ server */

typedef struct {
  tcpme_socket_t listen_sock;
  int error;
} ServerArg_t;

static TC_THREAD_FN server_thread(void *varg) {
  ServerArg_t *sa = varg;

  tcpme_set_t *set = tcpme_alloc_set(SET_CAP);
  assert(set != NULL);
  assert(tcpme_add_socket(set, sa->listen_sock) == 0);

  tcpme_socket_t clients[N_CLIENTS];
  int n_accepted = 0;
  int n_active = 0;

  /* The exit condition depends on every client connecting and then leaving. A
   * client that dies before connecting -- a refused or slow connect on a loaded
   * runner -- would otherwise strand this loop forever, and main() joins this
   * thread, so the whole test hangs until the harness kills it. Bound it. */
  const int64_t deadline = tc_now_ms() + SERVER_DEADLINE_MS;

  while (n_accepted < N_CLIENTS || n_active > 0) {
    if (tc_now_ms() > deadline) {
      fprintf(stderr, "server: gave up after %d ms (accepted %d/%d, %d still active)\n",
              SERVER_DEADLINE_MS, n_accepted, N_CLIENTS, n_active);
      sa->error = 1;
      break;
    }

    int ready = tcpme_check_sockets(set, 1000);
    if (ready < 0) {
      sa->error = 1;
      break;
    }

    if (n_accepted < N_CLIENTS && tcpme_socket_ready(set, sa->listen_sock)) {
      tcpme_socket_t c = tcpme_accept(sa->listen_sock);
      if (tcpme_socket_valid(c)) {
        clients[n_accepted++] = c;
        n_active++;
        printf("server: accepted %d/%d\n", n_accepted, N_CLIENTS);
        assert(tcpme_add_socket(set, c) == 0);
      }
    }

    for (int i = 0; i < n_accepted; i++) {
      if (!tcpme_socket_valid(clients[i]) || !tcpme_socket_ready(set, clients[i]))
        continue;

      char buf[64] = {0};
      int n = tcpme_recv(clients[i], buf, (int)sizeof(buf) - 1);
      if (n <= 0 || strcmp(buf, "QUIT") == 0) {
        tcpme_del_socket(set, clients[i]);
        tcpme_close(clients[i]);
        clients[i] = TCPME_INVALID_SOCKET;
        n_active--;
        continue;
      }

      /* "PING <id>:<round>" → "PONG <id>:<round>" */
      assert(strncmp(buf, "PING ", 5) == 0);
      char reply[64];
      int rlen = snprintf(reply, sizeof(reply), "PONG %s", buf + 5);
      assert(tcpme_send(clients[i], reply, rlen) == rlen);
    }
  }

  tcpme_free_set(set);
  return TC_THREAD_RET;
}

/* ------------------------------------------------------------------ client */

typedef struct {
  uint16_t port;
  int id;
  int error;
} ClientArg_t;

static TC_THREAD_FN client_thread(void *varg) {
  ClientArg_t *ca = varg;

  tcpme_socket_t sock = tcpme_connect("127.0.0.1", ca->port);
  if (!tcpme_socket_valid(sock))
    sock = tcpme_connect("::1", ca->port);
  if (!tcpme_socket_valid(sock)) {
    fprintf(stderr, "client %d: connect failed: %s\n", ca->id, tcpme_get_error());
    ca->error = 1;
    return TC_THREAD_RET;
  }

  /* Without a timeout a PONG that never arrives blocks this thread forever, and
   * main() is joining it. Bounded, the same loss is a failure naming the round. */
  tcpme_set_timeout(sock, CLIENT_IO_TIMEOUT_MS);
  printf("client %d: connected\n", ca->id);

  for (int r = 0; r < N_ROUNDS && !ca->error; r++) {
    char msg[32];
    int mlen = snprintf(msg, sizeof(msg), "PING %d:%d", ca->id, r);

    if (tcpme_send(sock, msg, mlen) != mlen) {
      fprintf(stderr, "client %d round %d: send failed: %s\n", ca->id, r, tcpme_get_error());
      ca->error = 1;
      break;
    }

    char reply[32] = {0};
    int n = tcpme_recv(sock, reply, (int)sizeof(reply) - 1);
    if (n <= 0) {
      fprintf(stderr, "client %d round %d: no PONG within %d ms (%s)\n", ca->id, r,
              CLIENT_IO_TIMEOUT_MS, tcpme_get_error());
      ca->error = 1;
      break;
    }

    char expected[32];
    snprintf(expected, sizeof(expected), "PONG %d:%d", ca->id, r);
    if (strcmp(reply, expected) != 0) {
      fprintf(stderr, "client %d round %d: expected \"%s\", got \"%s\"\n", ca->id, r, expected,
              reply);
      ca->error = 1;
    }
  }

  printf("client %d: done (%d rounds)\n", ca->id, N_ROUNDS);

  const char quit[] = "QUIT";
  tcpme_send(sock, quit, (int)strlen(quit));
  tcpme_close(sock);
  return TC_THREAD_RET;
}

/* ------------------------------------------------------------------ main */

int main(void) {
  tc_test_init();
  assert(tcpme_init() == 0);

  tcpme_socket_t listen_sock = tcpme_listen(NULL, 0);
  assert(tcpme_socket_valid(listen_sock));

  char addr_buf[TCPME_ADDRPORTSTRLEN];
  assert(tcpme_get_local_addr(listen_sock, addr_buf, sizeof(addr_buf)));
  uint16_t port = extract_port(addr_buf);
  printf("mini-server listening at %s\n", addr_buf);

  ServerArg_t sa = {.listen_sock = listen_sock, .error = 0};
  tc_thread_t server_tid;
  assert(tc_thread_create(&server_tid, server_thread, &sa) == 0);

  ClientArg_t ca[N_CLIENTS];
  tc_thread_t client_tids[N_CLIENTS];
  for (int i = 0; i < N_CLIENTS; i++) {
    ca[i] = (ClientArg_t){.port = port, .id = i, .error = 0};
    assert(tc_thread_create(&client_tids[i], client_thread, &ca[i]) == 0);
  }

  for (int i = 0; i < N_CLIENTS; i++)
    tc_thread_join(client_tids[i]);
  tc_thread_join(server_tid);

  for (int i = 0; i < N_CLIENTS; i++)
    assert(ca[i].error == 0);
  assert(sa.error == 0);

  tcpme_close(listen_sock);
  tcpme_quit();
  return 0;
}
