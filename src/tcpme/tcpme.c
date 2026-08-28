/*
 tcpme.c
 https://github.com/Dealer-s-Choice/dealers-choice

 MIT License

 Copyright (c) 2026 Andy Alt

 Written largely by Claude (an LLM by Anthropic) at Andy's direction.
*/

/*
  "tcpme" was inspired by libsdl_net <https://github.com/libsdl-org/SDL_net>
  A portable network library for use with SDL. It's goal is to simplify the
  use of the usual socket interfaces and use SDL infrastructure to handle
  some portability things (such as threading and reporting errors).
*/

#ifdef _WIN32
// clang-format off
#  include <winsock2.h>
#  include <ws2tcpip.h>
// clang-format on
#define close_socket(s) closesocket(s)
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <net/if.h> /* if_nameindex / if_nametoindex for IPv6 multicast joins */
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#define close_socket(s) close(s)
#endif

#include "tcpme.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- Error handling ---------------------------------------------------------

// MSVC does not implement _Thread_local even in /std:c11 mode.
#ifdef _MSC_VER
#define TCPME_TLS __declspec(thread)
#else
#define TCPME_TLS _Thread_local
#endif

static TCPME_TLS char tcpme_errbuf[256];

static void set_error(const char *msg) {
  strncpy(tcpme_errbuf, msg, sizeof(tcpme_errbuf) - 1);
  tcpme_errbuf[sizeof(tcpme_errbuf) - 1] = '\0';
}

/* Reject a negative length at a datagram API boundary before the (size_t)len
 * cast turns it into a huge value that POSIX sendto/recvfrom would service (OOB
 * read on send, overflow on recv) while Winsock rejects it — the same guard,
 * and reason, as tcpme_send/tcpme_recv. `errval` is the caller's error return
 * (-1, or 0 for the count-returning multicast sender). Zero-length datagrams
 * are valid, so only negatives are rejected. */
#define TCPME_REJECT_NEG_LEN(len, errval)                                                          \
  do {                                                                                             \
    if ((len) < 0) {                                                                               \
      set_error("negative length");                                                                \
      return (errval);                                                                             \
    }                                                                                              \
  } while (0)

static void set_nodelay(tcpme_socket_t sock) {
  /* Best-effort latency knob: without TCP_NODELAY the connection still works,
   * just with Nagle batching, so a failure here is deliberately ignored. */
#ifndef _WIN32
  int one = 1;
  (void)setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
#else
  DWORD one = 1;
  (void)setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char *)&one, sizeof(one));
#endif
}

static void set_nosigpipe(tcpme_socket_t sock) {
#ifdef SO_NOSIGPIPE
  int one = 1;
  setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#else
  (void)sock;
#endif
}

/* Toggle a socket's non-blocking mode. Returns 0 on success, -1 on error. */
static int set_nonblocking(tcpme_socket_t sock, int nonblock) {
#ifdef _WIN32
  u_long mode = nonblock ? 1 : 0;
  return ioctlsocket(sock, FIONBIO, &mode) == 0 ? 0 : -1;
#else
  int flags = fcntl(sock, F_GETFL, 0);
  if (flags < 0)
    return -1;
  if (nonblock)
    flags |= O_NONBLOCK;
  else
    flags &= ~O_NONBLOCK;
  return fcntl(sock, F_SETFL, flags) == 0 ? 0 : -1;
#endif
}

static void set_error_sys(const char *prefix) {
#ifdef _WIN32
  int err = WSAGetLastError();
  char msg[200] = "";
  FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, (DWORD)err,
                 MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), msg, (DWORD)sizeof(msg), NULL);
  int len = (int)strlen(msg);
  while (len > 0 && (msg[len - 1] == '\r' || msg[len - 1] == '\n'))
    msg[--len] = '\0';
  snprintf(tcpme_errbuf, sizeof(tcpme_errbuf), "%s: %s (%d)", prefix, msg, err);
#else
  /* strerror() may return a pointer into a shared static buffer, so two threads
   * formatting an error can race even though tcpme_errbuf is thread-local.
   * strerror_r has two incompatible shapes: the GNU one returns char* and may
   * leave the caller's buffer untouched, the XSI one returns int. Pick by the
   * macros glibc sets, and fall back to strerror() where neither applies. */
  char errmsg[128];
#  if defined(__GLIBC__) && (_GNU_SOURCE + 0) && !((_POSIX_C_SOURCE + 0) >= 200112L && !(_GNU_SOURCE + 0))
  /* GNU: the return value is the message, which may or may not be errmsg. */
  const char *m = strerror_r(errno, errmsg, sizeof(errmsg));
  snprintf(tcpme_errbuf, sizeof(tcpme_errbuf), "%s: %s", prefix, m ? m : "unknown error");
#  elif (_POSIX_C_SOURCE + 0) >= 200112L || defined(__APPLE__) || defined(__FreeBSD__) ||           \
      defined(__OpenBSD__) || defined(__NetBSD__) || defined(__DragonFly__)
  /* XSI: fills the buffer and returns 0 on success. */
  if (strerror_r(errno, errmsg, sizeof(errmsg)) != 0)
    snprintf(errmsg, sizeof(errmsg), "errno %d", errno);
  snprintf(tcpme_errbuf, sizeof(tcpme_errbuf), "%s: %s", prefix, errmsg);
#  else
  snprintf(tcpme_errbuf, sizeof(tcpme_errbuf), "%s: %s", prefix, strerror(errno));
#  endif
#endif
}

const char *tcpme_get_error(void) { return tcpme_errbuf; }

// --- Init / quit ------------------------------------------------------------

int tcpme_init(void) {
#ifdef _WIN32
  WSADATA wsa;
  if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
    set_error_sys("WSAStartup failed");
    return -1;
  }
#endif
  return 0;
}

void tcpme_quit(void) {
#ifdef _WIN32
  WSACleanup();
#endif
}

// --- Internal helpers -------------------------------------------------------

static bool format_sockaddr(const struct sockaddr *sa, socklen_t salen, bool with_port, char *buf,
                            size_t buflen) {
  char host[INET6_ADDRSTRLEN];
  char port[8];
  int flags = NI_NUMERICHOST | NI_NUMERICSERV;
  int rc = getnameinfo(sa, salen, host, sizeof(host), with_port ? port : NULL,
                       with_port ? (socklen_t)sizeof(port) : 0, flags);
  if (rc != 0) {
    set_error(gai_strerror(rc));
    return false;
  }
  if (!with_port) {
    /* snprintf (not strncpy) so buflen == 0 is safe — strncpy's buflen-1 would
     * underflow to SIZE_MAX. Matches the bounded with_port branch below. */
    snprintf(buf, buflen, "%s", host);
    return true;
  }
  if (sa->sa_family == AF_INET6)
    snprintf(buf, buflen, "[%s]:%s", host, port);
  else
    snprintf(buf, buflen, "%s:%s", host, port);
  return true;
}

/* Resolve host:port for a TCP socket (SOCK_STREAM, AI_NUMERICSERV; AI_PASSIVE
 * when `passive`, i.e. for listen). On success fills *res — the caller must
 * freeaddrinfo it — and returns 0; on failure sets the error and returns -1. */
static int resolve_tcp(const char *host, uint16_t port, bool passive, struct addrinfo **res) {
  char portstr[8];
  snprintf(portstr, sizeof(portstr), "%u", (unsigned)port);

  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_NUMERICSERV | (passive ? AI_PASSIVE : 0);

  *res = NULL;
  int rc = getaddrinfo(host, portstr, &hints, res);
  if (rc != 0) {
    set_error(gai_strerror(rc));
    return -1;
  }
  return 0;
}

// --- Listen / accept / connect ----------------------------------------------

tcpme_socket_t tcpme_listen(const char *host, uint16_t port) {
  struct addrinfo *res = NULL;
  if (resolve_tcp(host, port, true, &res) != 0)
    return TCPME_INVALID_SOCKET;

  static const int families[2] = {AF_INET6, AF_INET};
  tcpme_socket_t sock = TCPME_INVALID_SOCKET;
  for (int fi = 0; fi < 2 && sock == TCPME_INVALID_SOCKET; fi++) {
    for (struct addrinfo *ai = res; ai != NULL; ai = ai->ai_next) {
      if (ai->ai_family != families[fi])
        continue;

      sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
      if (sock == TCPME_INVALID_SOCKET)
        continue;

      int one = 1;
      /* Best-effort: without SO_REUSEADDR a quick restart may fail to bind
       * (TIME_WAIT), but the listen itself still works, so don't abort on it. */
      (void)setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&one, sizeof(one));
#ifdef IPV6_V6ONLY
      if (ai->ai_family == AF_INET6) {
        int zero = 0;
        /* A failure here is caught by the read-back verify just below. */
        (void)setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, (const char *)&zero, sizeof(zero));
        // Verify dual-stack actually took effect; OpenBSD ignores IPV6_V6ONLY=0
        // and the socket would only accept IPv6, so fall through to the IPv4 pass.
        // A failed getsockopt leaves v6only at 1 -> treated as not-dual-stack.
        int v6only = 1;
        socklen_t vlen = sizeof(v6only);
        if (getsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&v6only, &vlen) != 0 ||
            v6only != 0) {
          close_socket(sock);
          sock = TCPME_INVALID_SOCKET;
          continue;
        }
      }
#endif

      if (bind(sock, ai->ai_addr, (socklen_t)ai->ai_addrlen) != 0 || listen(sock, SOMAXCONN) != 0) {
        close_socket(sock);
        sock = TCPME_INVALID_SOCKET;
        continue;
      }
      break;
    }
  }

  freeaddrinfo(res);

  if (sock == TCPME_INVALID_SOCKET)
    set_error_sys("Failed to bind/listen");

  return sock;
}

tcpme_socket_t tcpme_accept(tcpme_socket_t server_sock) {
  if (!tcpme_socket_valid(server_sock))
    return TCPME_INVALID_SOCKET;

#ifndef _WIN32
  /* POSIX fd_set is a bitmap indexed by fd value; guard against out-of-range. */
  if ((int)server_sock >= FD_SETSIZE) {
    set_error("accept: socket fd exceeds FD_SETSIZE");
    return TCPME_INVALID_SOCKET;
  }
#endif

  // Use select with zero timeout so this never blocks.
  fd_set rfds;
  FD_ZERO(&rfds);
  FD_SET(server_sock, &rfds);
  struct timeval tv = {0, 0};

  int n = select(
#ifdef _WIN32
      0,
#else
      (int)server_sock + 1,
#endif
      &rfds, NULL, NULL, &tv);

  if (n < 0) {
    set_error_sys("accept: select() failed");
    return TCPME_INVALID_SOCKET;
  }
  if (n == 0)
    return TCPME_INVALID_SOCKET;

  struct sockaddr_storage addr;
  socklen_t addrlen = sizeof(addr);
  tcpme_socket_t client = accept(server_sock, (struct sockaddr *)&addr, &addrlen);
  if (client == TCPME_INVALID_SOCKET) {
    set_error_sys("accept() failed");
    return TCPME_INVALID_SOCKET;
  }
  set_nodelay(client);
  set_nosigpipe(client);
  return client;
}

tcpme_socket_t tcpme_connect(const char *host, uint16_t port) {
  struct addrinfo *res = NULL;
  if (resolve_tcp(host, port, false, &res) != 0)
    return TCPME_INVALID_SOCKET;

  tcpme_socket_t sock = TCPME_INVALID_SOCKET;
  for (struct addrinfo *ai = res; ai != NULL; ai = ai->ai_next) {
    sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (sock == TCPME_INVALID_SOCKET)
      continue;
    if (connect(sock, ai->ai_addr, (socklen_t)ai->ai_addrlen) != 0) {
      close_socket(sock);
      sock = TCPME_INVALID_SOCKET;
      continue;
    }
    set_nodelay(sock);
    set_nosigpipe(sock);
    break;
  }

  freeaddrinfo(res);

  if (sock == TCPME_INVALID_SOCKET)
    set_error_sys("Failed to connect");

  return sock;
}

/* Attempt one resolved address with a connect timeout. Returns a ready blocking
 * socket, or TCPME_INVALID_SOCKET on failure. Factored out so callers can walk
 * the addrinfo list in different orders (plain order, or IPv4-first). */
static tcpme_socket_t try_connect_addrinfo(const struct addrinfo *ai, uint32_t timeout_ms) {
  tcpme_socket_t sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
  if (sock == TCPME_INVALID_SOCKET)
    return TCPME_INVALID_SOCKET;
  if (set_nonblocking(sock, 1) != 0) {
    close_socket(sock);
    return TCPME_INVALID_SOCKET;
  }
#ifndef _WIN32
  /* select() indexes a POSIX fd_set bitmap by fd value, so an fd >= FD_SETSIZE
   * is out of bounds. (Winsock's fd_set is a bounded array, not indexed by
   * value, so this guard is POSIX-only.) */
  if ((int)sock >= FD_SETSIZE) {
    set_error("connect: socket fd exceeds FD_SETSIZE");
    close_socket(sock);
    return TCPME_INVALID_SOCKET;
  }
#endif

  bool connected = (connect(sock, ai->ai_addr, (socklen_t)ai->ai_addrlen) == 0);
  if (!connected) {
#ifdef _WIN32
    bool in_progress = (WSAGetLastError() == WSAEWOULDBLOCK);
#else
    bool in_progress = (errno == EINPROGRESS);
#endif
    if (in_progress) {
      /* Wait for the connect to resolve. POSIX reports both success AND
       * failure by marking the socket writable (SO_ERROR then tells them
       * apart), but Winsock reports failure ONLY in the except set -- with
       * just writefds, a refused connect on Windows never wakes select and
       * silently eats the whole timeout budget. Pass both sets. */
      fd_set wfds, efds;
      int n;
      /* Retry across EINTR so a stray signal doesn't turn an in-progress
       * connect into a spurious failure. A repeated signal can extend the wait
       * by up to timeout_ms per interruption; connect_timeout's budget is
       * already per-address, not a hard overall deadline, so that's acceptable. */
      for (;;) {
        FD_ZERO(&wfds);
        FD_ZERO(&efds);
        FD_SET(sock, &wfds);
        FD_SET(sock, &efds);
        struct timeval tv;
        tv.tv_sec = (long)(timeout_ms / 1000);
        tv.tv_usec = (long)((timeout_ms % 1000) * 1000);
        n = select(
#ifdef _WIN32
            0,
#else
            (int)sock + 1,
#endif
            NULL, &wfds, &efds, &tv);
        if (n < 0) {
#ifdef _WIN32
          if (WSAGetLastError() == WSAEINTR)
            continue;
#else
          if (errno == EINTR)
            continue;
#endif
        }
        break;
      }
      if (n > 0 && (FD_ISSET(sock, &wfds) || FD_ISSET(sock, &efds))) {
        int so_error = 0;
        socklen_t len = sizeof(so_error);
        if (getsockopt(sock, SOL_SOCKET, SO_ERROR, (char *)&so_error, &len) == 0) {
          if (so_error == 0) {
            connected = true;
          } else {
            /* Surface the real failure (e.g. ECONNREFUSED) so the caller's
             * set_error_sys doesn't format a stale, unrelated error code. */
#ifdef _WIN32
            WSASetLastError(so_error);
#else
            errno = so_error;
#endif
          }
        }
      }
    }
  }

  if (connected) {
    /* Restore blocking mode for normal send/recv; if that fails the socket
     * would give spurious EWOULDBLOCK later, so fail the connect instead. */
    if (set_nonblocking(sock, 0) != 0) {
      set_error_sys("connect: failed to restore blocking mode");
      close_socket(sock);
      return TCPME_INVALID_SOCKET;
    }
    set_nodelay(sock);
    set_nosigpipe(sock);
    return sock;
  }
  close_socket(sock);
  return TCPME_INVALID_SOCKET;
}

/* Shared body of the two timed-connect entry points. `pref4` selects an
 * IPv4-first two-pass walk of the resolved addresses; otherwise they're tried
 * in resolver order. timeout_ms applies per address (see the header). */
static tcpme_socket_t connect_timeout_impl(const char *host, uint16_t port, uint32_t timeout_ms,
                                           bool pref4) {
  struct addrinfo *res = NULL;
  if (resolve_tcp(host, port, false, &res) != 0)
    return TCPME_INVALID_SOCKET;

  tcpme_socket_t sock = TCPME_INVALID_SOCKET;
  if (pref4) {
    /* Two passes: IPv4 first, then everything else, so a dual-stack host is
     * reached over IPv4 when it has it and falls back to IPv6 only otherwise. */
    for (int v4_pass = 1; v4_pass >= 0 && !tcpme_socket_valid(sock); v4_pass--) {
      for (struct addrinfo *ai = res; ai != NULL; ai = ai->ai_next) {
        bool is_v4 = (ai->ai_family == AF_INET);
        if (is_v4 != (v4_pass == 1))
          continue;
        sock = try_connect_addrinfo(ai, timeout_ms);
        if (tcpme_socket_valid(sock))
          break;
      }
    }
  } else {
    for (struct addrinfo *ai = res; ai != NULL; ai = ai->ai_next) {
      sock = try_connect_addrinfo(ai, timeout_ms);
      if (tcpme_socket_valid(sock))
        break;
    }
  }

  freeaddrinfo(res);

  if (sock == TCPME_INVALID_SOCKET)
    set_error_sys("connect timed out or failed");

  return sock;
}

tcpme_socket_t tcpme_connect_timeout(const char *host, uint16_t port, uint32_t timeout_ms) {
  return connect_timeout_impl(host, port, timeout_ms, false);
}

tcpme_socket_t tcpme_connect_timeout_pref4(const char *host, uint16_t port, uint32_t timeout_ms) {
  return connect_timeout_impl(host, port, timeout_ms, true);
}

// --- Close / send / recv ----------------------------------------------------

void tcpme_close(tcpme_socket_t sock) {
  if (sock != TCPME_INVALID_SOCKET)
    close_socket(sock);
}

int tcpme_send(tcpme_socket_t sock, const void *buf, int len) {
  /* Reject degenerate lengths at the API boundary. On POSIX a negative int
   * converts to a huge size_t, which Linux clamps (~2 GB) and services rather
   * than rejecting — send() could then block or transmit garbage instead of
   * failing. Winsock fails it cleanly; make both platforms behave the same. */
  if (len < 0) {
    set_error("send: negative length");
    return -1;
  }
  if (len == 0)
    return 0;
  int n;
#if defined(MSG_NOSIGNAL)
  do {
    n = (int)send(sock, (const char *)buf, len, MSG_NOSIGNAL);
  } while (n < 0 && errno == EINTR);
#elif !defined(_WIN32)
  do {
    n = (int)send(sock, (const char *)buf, len, 0);
  } while (n < 0 && errno == EINTR);
#else
  n = (int)send(sock, (const char *)buf, len, 0);
#endif
  if (n < 0)
    set_error_sys("send() failed");
  return n;
}

int tcpme_recv(tcpme_socket_t sock, void *buf, int len) {
  /* Same boundary guard as tcpme_send: a negative len becomes a huge size_t
   * on POSIX and recv() blocks waiting for data instead of failing. */
  if (len < 0) {
    set_error("recv: negative length");
    return -1;
  }
  if (len == 0)
    return 0;
  int n;
#ifndef _WIN32
  do {
    n = (int)recv(sock, (char *)buf, len, 0);
  } while (n < 0 && errno == EINTR);
#else
  n = (int)recv(sock, (char *)buf, len, 0);
#endif
  if (n < 0)
    set_error_sys("recv() failed");
  return n;
}

int tcpme_set_timeout(tcpme_socket_t sock, uint32_t timeout_ms) {
#ifdef _WIN32
  DWORD val = (DWORD)timeout_ms;
  const void *optval = &val;
  socklen_t optlen = sizeof(val);
#else
  struct timeval val;
  val.tv_sec = timeout_ms / 1000;
  val.tv_usec = (timeout_ms % 1000) * 1000;
  const void *optval = &val;
  socklen_t optlen = sizeof(val);
#endif
  if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)optval, optlen) != 0 ||
      setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char *)optval, optlen) != 0) {
    set_error_sys("setsockopt(timeout) failed");
    return -1;
  }
  return 0;
}

// --- Address queries --------------------------------------------------------

/* Shared body of the address-query functions: read the socket's local
 * (`local`) or peer name, then format it with or without the port. */
static bool query_addr(tcpme_socket_t sock, bool local, bool with_port, char *buf, size_t buflen) {
  struct sockaddr_storage sa;
  socklen_t salen = sizeof(sa);
  int rc = local ? getsockname(sock, (struct sockaddr *)&sa, &salen)
                 : getpeername(sock, (struct sockaddr *)&sa, &salen);
  if (rc != 0) {
    set_error_sys(local ? "getsockname() failed" : "getpeername() failed");
    return false;
  }
  return format_sockaddr((const struct sockaddr *)&sa, salen, with_port, buf, buflen);
}

bool tcpme_get_peer_addr(tcpme_socket_t sock, char *buf, size_t buflen) {
  return query_addr(sock, false, true, buf, buflen);
}

bool tcpme_get_local_addr(tcpme_socket_t sock, char *buf, size_t buflen) {
  return query_addr(sock, true, true, buf, buflen);
}

bool tcpme_get_peer_ip(tcpme_socket_t sock, char *buf, size_t buflen) {
  return query_addr(sock, false, false, buf, buflen);
}

// --- UDP (IPv4 datagram) ----------------------------------------------------

tcpme_socket_t tcpme_udp_open(uint16_t bind_port, bool broadcast) {
  tcpme_socket_t sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock == TCPME_INVALID_SOCKET) {
    set_error_sys("udp: socket() failed");
    return TCPME_INVALID_SOCKET;
  }

  int one = 1;
  /* Best-effort; see tcpme_listen. */
  (void)setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&one, sizeof(one));

  if (broadcast &&
      setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (const char *)&one, sizeof(one)) != 0) {
    set_error_sys("udp: enable SO_BROADCAST failed");
    close_socket(sock);
    return TCPME_INVALID_SOCKET;
  }

  // Always bind so the socket is immediately able to receive. bind_port=0
  // lets the kernel assign an ephemeral port (read it back with
  // tcpme_get_local_addr if needed).
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(bind_port);
  if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
    set_error_sys("udp: bind() failed");
    close_socket(sock);
    return TCPME_INVALID_SOCKET;
  }

  return sock;
}

int tcpme_udp_broadcast(tcpme_socket_t sock, uint16_t port, const void *buf, int len) {
  TCPME_REJECT_NEG_LEN(len, -1);
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
  addr.sin_port = htons(port);
  int n = (int)sendto(sock, (const char *)buf, (size_t)len, 0, (struct sockaddr *)&addr,
                      sizeof(addr));
  if (n < 0)
    set_error_sys("udp: broadcast sendto() failed");
  return n;
}

int tcpme_udp_sendto(tcpme_socket_t sock, const char *ip, uint16_t port, const void *buf, int len) {
  TCPME_REJECT_NEG_LEN(len, -1);
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) {
    set_error("udp: invalid IPv4 address");
    return -1;
  }
  int n = (int)sendto(sock, (const char *)buf, (size_t)len, 0, (struct sockaddr *)&addr,
                      sizeof(addr));
  if (n < 0)
    set_error_sys("udp: sendto() failed");
  return n;
}

int tcpme_udp_recvfrom(tcpme_socket_t sock, void *buf, int len, char *out_ip, size_t out_iplen,
                       uint16_t *out_port) {
  TCPME_REJECT_NEG_LEN(len, -1);
  struct sockaddr_in src;
  socklen_t srclen = sizeof(src);
  int n;
#ifndef _WIN32
  do {
    n = (int)recvfrom(sock, (char *)buf, (size_t)len, 0, (struct sockaddr *)&src, &srclen);
  } while (n < 0 && errno == EINTR);
#else
  n = (int)recvfrom(sock, (char *)buf, len, 0, (struct sockaddr *)&src, &srclen);
#endif
  if (n < 0) {
    set_error_sys("udp: recvfrom() failed");
    return -1;
  }
  if (out_ip && out_iplen > 0) {
    if (!inet_ntop(AF_INET, &src.sin_addr, out_ip, (socklen_t)out_iplen))
      out_ip[0] = '\0';
  }
  if (out_port)
    *out_port = ntohs(src.sin_port);
  return n;
}

// --- UDP (IPv6 + multicast, e.g. for LAN multicast discovery) ----------------
//
// IPv6 has no broadcast; discovery on a link uses multicast instead. These
// mirror the IPv4 UDP helpers above but on an AF_INET6 socket, plus the
// multicast plumbing (group join, per-interface send) link-local discovery
// needs. The socket is V6ONLY so a host can run an IPv4 *and* an IPv6 discovery
// socket on the same port without one stealing the other's datagrams.

// IPV6_JOIN_GROUP is the RFC 3493 name; some stacks only define the older alias.
#if !defined(IPV6_JOIN_GROUP) && defined(IPV6_ADD_MEMBERSHIP)
#define IPV6_JOIN_GROUP IPV6_ADD_MEMBERSHIP
#endif

tcpme_socket_t tcpme_udp_open6(uint16_t bind_port) {
  tcpme_socket_t sock = socket(AF_INET6, SOCK_DGRAM, 0);
  if (sock == TCPME_INVALID_SOCKET) {
    set_error_sys("udp6: socket() failed");
    return TCPME_INVALID_SOCKET;
  }

  int one = 1;
  /* Best-effort; see tcpme_listen. */
  (void)setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&one, sizeof(one));
  // Keep this socket purely IPv6 so it can coexist with a separate IPv4 UDP
  // socket bound to the same port (the dual-stack discovery setup). Best-effort:
  // if it fails (v6only stays off) the worst case is this socket also receiving
  // v4-mapped datagrams meant for the companion IPv4 socket.
  (void)setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, (const char *)&one, sizeof(one));
  // Link-local scope (1 hop) and loop multicast back, so a server and client on
  // the same host still discover each other. Both best-effort: 1 hop is already
  // the RFC 3493 default for MULTICAST_HOPS, and loopback defaults to on.
  int hops = 1;
  (void)setsockopt(sock, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, (const char *)&hops, sizeof(hops));
  (void)setsockopt(sock, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, (const char *)&one, sizeof(one));

  struct sockaddr_in6 addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin6_family = AF_INET6;
  addr.sin6_addr = in6addr_any;
  addr.sin6_port = htons(bind_port);
  if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
    set_error_sys("udp6: bind() failed");
    close_socket(sock);
    return TCPME_INVALID_SOCKET;
  }
  return sock;
}

// Join `group` (a numeric IPv6 multicast address) on every usable interface.
// Link-local groups (ff02::/16) are per-link, so receiving on more than one NIC
// requires a membership per interface. Returns the number of joins that
// succeeded (0 means the socket will hear nothing).
int tcpme_mcast6_join_all(tcpme_socket_t sock, const char *group) {
  struct ipv6_mreq mreq;
  memset(&mreq, 0, sizeof(mreq));
  if (inet_pton(AF_INET6, group, &mreq.ipv6mr_multiaddr) != 1) {
    set_error("udp6: invalid multicast group");
    return 0;
  }
  int joined = 0;
#ifndef _WIN32
  struct if_nameindex *ifs = if_nameindex();
  if (ifs) {
    for (struct if_nameindex *p = ifs; p->if_index != 0 || p->if_name != NULL; p++) {
      mreq.ipv6mr_interface = p->if_index;
      if (setsockopt(sock, IPPROTO_IPV6, IPV6_JOIN_GROUP, (const char *)&mreq, sizeof(mreq)) == 0)
        joined++;
    }
    if_freenameindex(ifs);
  }
#endif
  // Also join on the default interface (index 0). On Windows (no if_nameindex
  // enumeration here) this is the only join attempted.
  mreq.ipv6mr_interface = 0;
  if (setsockopt(sock, IPPROTO_IPV6, IPV6_JOIN_GROUP, (const char *)&mreq, sizeof(mreq)) == 0)
    joined++;
  if (joined == 0)
    set_error_sys("udp6: no multicast group joins succeeded");
  return joined;
}

// Send `buf` to `group`:`port` out every interface (setting IPV6_MULTICAST_IF per
// NIC and the destination scope), so a query reaches servers on each attached
// link. Returns the number of interfaces the datagram went out on.
int tcpme_udp_mcast6_send_all(tcpme_socket_t sock, const char *group, uint16_t port,
                              const void *buf, int len) {
  TCPME_REJECT_NEG_LEN(len, 0); /* returns an interface count; 0 = sent nowhere */
  struct sockaddr_in6 addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin6_family = AF_INET6;
  addr.sin6_port = htons(port);
  if (inet_pton(AF_INET6, group, &addr.sin6_addr) != 1) {
    set_error("udp6: invalid multicast group");
    return 0;
  }
  int sent = 0;
#ifndef _WIN32
  struct if_nameindex *ifs = if_nameindex();
  if (ifs) {
    for (struct if_nameindex *p = ifs; p->if_index != 0 || p->if_name != NULL; p++) {
      unsigned idx = p->if_index;
      // If the interface can't be selected (down, no IPv6, ...) skip it rather
      // than send out whatever interface the socket was last bound to.
      if (setsockopt(sock, IPPROTO_IPV6, IPV6_MULTICAST_IF, (const char *)&idx, sizeof(idx)) != 0)
        continue;
      addr.sin6_scope_id = idx; // link-local destination needs the outgoing scope
      if ((int)sendto(sock, (const char *)buf, (size_t)len, 0, (struct sockaddr *)&addr,
                      sizeof(addr)) == len)
        sent++;
    }
    if_freenameindex(ifs);
  }
#endif
  if (sent == 0) {
    // Fallback: default interface (and the only path on Windows here).
    // Best-effort: index 0 = "kernel picks"; if even that fails, the sendto
    // below reports the error.
    unsigned idx = 0;
    (void)setsockopt(sock, IPPROTO_IPV6, IPV6_MULTICAST_IF, (const char *)&idx, sizeof(idx));
    addr.sin6_scope_id = 0;
    if ((int)sendto(sock, (const char *)buf, (size_t)len, 0, (struct sockaddr *)&addr,
                    sizeof(addr)) == len)
      sent++;
  }
  if (sent == 0)
    set_error_sys("udp6: multicast sendto() failed on all interfaces");
  return sent;
}

// Like tcpme_udp_recvfrom but for IPv6, and also returns the sender's scope id
// (the interface index for a link-local fe80:: source) — needed to reply to, or
// later connect to, a link-local address.
int tcpme_udp_recvfrom6(tcpme_socket_t sock, void *buf, int len, char *out_ip, size_t out_iplen,
                        unsigned *out_scope, uint16_t *out_port) {
  TCPME_REJECT_NEG_LEN(len, -1);
  struct sockaddr_in6 src;
  socklen_t srclen = sizeof(src);
  int n;
#ifndef _WIN32
  do {
    n = (int)recvfrom(sock, (char *)buf, (size_t)len, 0, (struct sockaddr *)&src, &srclen);
  } while (n < 0 && errno == EINTR);
#else
  n = (int)recvfrom(sock, (char *)buf, len, 0, (struct sockaddr *)&src, &srclen);
#endif
  if (n < 0) {
    set_error_sys("udp6: recvfrom() failed");
    return -1;
  }
  if (out_ip && out_iplen > 0) {
    if (!inet_ntop(AF_INET6, &src.sin6_addr, out_ip, (socklen_t)out_iplen))
      out_ip[0] = '\0';
  }
  if (out_scope)
    *out_scope = src.sin6_scope_id;
  if (out_port)
    *out_port = ntohs(src.sin6_port);
  return n;
}

// Unicast `buf` to an IPv6 address, carrying the scope id for a link-local
// destination (0 for global/ULA addresses).
int tcpme_udp_sendto6(tcpme_socket_t sock, const char *ip, unsigned scope, uint16_t port,
                      const void *buf, int len) {
  TCPME_REJECT_NEG_LEN(len, -1);
  struct sockaddr_in6 addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin6_family = AF_INET6;
  addr.sin6_port = htons(port);
  addr.sin6_scope_id = scope;
  if (inet_pton(AF_INET6, ip, &addr.sin6_addr) != 1) {
    set_error("udp6: invalid IPv6 address");
    return -1;
  }
  int n = (int)sendto(sock, (const char *)buf, (size_t)len, 0, (struct sockaddr *)&addr,
                      sizeof(addr));
  if (n < 0)
    set_error_sys("udp6: sendto() failed");
  return n;
}

// --- Socket set -------------------------------------------------------------

struct tcpme_set_t {
  tcpme_socket_t *sockets;
  bool *ready;
  int count;
  int capacity;
};

tcpme_set_t *tcpme_alloc_set(int capacity) {
  if (capacity <= 0) {
    set_error("capacity must be > 0");
    return NULL;
  }
  /* The readiness layer is select()-based: a POSIX fd_set holds fds < FD_SETSIZE
   * and the Winsock fd_set array holds at most FD_SETSIZE entries, so a larger
   * set can't be select()ed safely. A poll()/WSAPoll migration would lift this
   * (see BACKLOG). */
  if (capacity > FD_SETSIZE) {
    set_error("capacity exceeds FD_SETSIZE");
    return NULL;
  }
  tcpme_set_t *set = (tcpme_set_t *)malloc(sizeof(*set));
  if (!set) {
    set_error_sys("tcpme_alloc_set: malloc failed");
    return NULL;
  }
  set->sockets = (tcpme_socket_t *)malloc((size_t)capacity * sizeof(tcpme_socket_t));
  set->ready = (bool *)calloc((size_t)capacity, sizeof(bool));
  if (!set->sockets || !set->ready) {
    set_error_sys("tcpme_alloc_set: malloc failed");
    free(set->sockets);
    free(set->ready);
    free(set);
    return NULL;
  }
  set->count = 0;
  set->capacity = capacity;
  return set;
}

void tcpme_free_set(tcpme_set_t *set) {
  if (set) {
    free(set->sockets);
    free(set->ready);
    free(set);
  }
}

int tcpme_add_socket(tcpme_set_t *set, tcpme_socket_t sock) {
  if (!set) {
    set_error("tcpme_add_socket: NULL socket set");
    return -1;
  }
  if (set->count >= set->capacity) {
    set_error("socket set is full");
    return -1;
  }
  set->sockets[set->count] = sock;
  set->ready[set->count] = false;
  set->count++;
  return 0;
}

int tcpme_del_socket(tcpme_set_t *set, tcpme_socket_t sock) {
  if (!set) {
    set_error("tcpme_del_socket: NULL socket set");
    return -1;
  }
  for (int i = 0; i < set->count; i++) {
    if (set->sockets[i] == sock) {
      set->count--;
      set->sockets[i] = set->sockets[set->count];
      set->ready[i] = set->ready[set->count];
      return 0;
    }
  }
  set_error("socket not found in set");
  return -1;
}

int tcpme_check_sockets(tcpme_set_t *set, uint32_t timeout_ms) {
  if (!set || set->count == 0)
    return 0;

  fd_set rfds;
  FD_ZERO(&rfds);
  int nfds = 0;
  for (int i = 0; i < set->count; i++) {
#ifndef _WIN32
    /* POSIX fd_set is a bitmap indexed by fd value; an fd >= FD_SETSIZE would
     * corrupt the stack. (Winsock's fd_set is a count-bounded array — the cap
     * in tcpme_alloc_set covers that side.) */
    if ((int)set->sockets[i] >= FD_SETSIZE) {
      set_error("tcpme_check_sockets: socket fd exceeds FD_SETSIZE");
      return -1;
    }
    if ((int)set->sockets[i] > nfds)
      nfds = (int)set->sockets[i];
#endif
    FD_SET(set->sockets[i], &rfds);
  }

  struct timeval tv;
  tv.tv_sec = (long)(timeout_ms / 1000);
  tv.tv_usec = (long)((timeout_ms % 1000) * 1000);

  int n = select(nfds + 1, &rfds, NULL, NULL, &tv);
  if (n < 0) {
#ifndef _WIN32
    if (errno == EINTR)
      return 0;
#endif
    set_error_sys("select() failed");
    return -1;
  }

  for (int i = 0; i < set->count; i++)
    set->ready[i] = (FD_ISSET(set->sockets[i], &rfds) != 0);

  return n;
}

bool tcpme_socket_ready(const tcpme_set_t *set, tcpme_socket_t sock) {
  if (!set)
    return false;
  for (int i = 0; i < set->count; i++) {
    if (set->sockets[i] == sock)
      return set->ready[i];
  }
  return false;
}
