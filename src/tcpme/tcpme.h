/*
 tcpme.h
 https://github.com/Dealer-s-Choice/dealers-choice

 MIT License

 Copyright (c) 2026 Andy Alt

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
*/

/*
  "tcpme" was inspired by libsdl_net <https://github.com/libsdl-org/SDL_net>
  A portable network library for use with SDL. It's goal is to simplify the
  use of the usual socket interfaces and use SDL infrastructure to handle
  some portability things (such as threading and reporting errors).
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
// clang-format off
#  include <winsock2.h>
#  include <ws2tcpip.h>
// clang-format on
typedef SOCKET tcpme_socket_t;
#define TCPME_INVALID_SOCKET INVALID_SOCKET
#else
#include <netinet/in.h>
typedef int tcpme_socket_t;
#define TCPME_INVALID_SOCKET ((tcpme_socket_t)(-1))
#endif

static inline bool tcpme_socket_valid(tcpme_socket_t s) { return s != TCPME_INVALID_SOCKET; }

/* Big-endian byte packing for protocol headers. Pure shifts, so the result is
 * correct on any host endianness; read/write directly to/from a byte buffer. */
static inline void tcpme_put_be16(uint8_t *p, uint16_t v) {
  p[0] = (uint8_t)(v >> 8);
  p[1] = (uint8_t)v;
}

static inline void tcpme_put_be32(uint8_t *p, uint32_t v) {
  p[0] = (uint8_t)(v >> 24);
  p[1] = (uint8_t)(v >> 16);
  p[2] = (uint8_t)(v >> 8);
  p[3] = (uint8_t)v;
}

static inline uint16_t tcpme_get_be16(const uint8_t *p) {
  return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

static inline uint32_t tcpme_get_be32(const uint8_t *p) {
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) |
         (uint32_t)p[3];
}

// Buffer size sufficient for a numeric IP address (no port) string.
#define TCPME_ADDRSTRLEN INET6_ADDRSTRLEN

// Buffer size sufficient for "IP:port" or "[IPv6]:port" strings.
#define TCPME_ADDRPORTSTRLEN (TCPME_ADDRSTRLEN + 8)

typedef struct tcpme_set_t tcpme_set_t;

// On Windows: calls WSAStartup. No-op on other platforms.
// Must be called before any other tcpme function.
int tcpme_init(void);

// On Windows: calls WSACleanup. No-op elsewhere.
void tcpme_quit(void);

// Returns the description of the last tcpme error on this thread.
const char *tcpme_get_error(void);

// Create a listening TCP socket bound to host:port.
// host=NULL binds to all interfaces (INADDR_ANY / in6addr_any).
// Passing a specific address or hostname binds to that interface only,
// enabling bind-address selection (e.g. "127.0.0.1"). Supports both
// IPv4 and IPv6 addresses and hostnames.
// Returns TCPME_INVALID_SOCKET on failure.
tcpme_socket_t tcpme_listen(const char *host, uint16_t port);

// Accept a pending incoming connection without blocking.
// Returns TCPME_INVALID_SOCKET if no connection is pending (not an error);
// use tcpme_get_error() only if you have independently detected a socket error.
tcpme_socket_t tcpme_accept(tcpme_socket_t server_sock);

// Connect to host:port (blocking).
// Resolves hostname; tries each resolved address (IPv4 and IPv6) in order.
// Returns TCPME_INVALID_SOCKET on failure.
tcpme_socket_t tcpme_connect(const char *host, uint16_t port);

// Close a socket.
void tcpme_close(tcpme_socket_t sock);

// Send len bytes from buf. Returns bytes sent, or -1 on error.
int tcpme_send(tcpme_socket_t sock, const void *buf, int len);

// Receive up to len bytes into buf.
// Returns bytes received, 0 on clean disconnect, or -1 on error.
int tcpme_recv(tcpme_socket_t sock, void *buf, int len);

// Set the send and receive timeout for a connected socket, in milliseconds.
// A blocking send/recv that makes no progress for this long then fails with
// an error (-1) instead of blocking indefinitely. timeout_ms = 0 disables the
// timeout (block indefinitely, the default). Returns 0 on success, -1 on error.
int tcpme_set_timeout(tcpme_socket_t sock, uint32_t timeout_ms);

// Write the peer's "IP:port" (IPv4) or "[IP]:port" (IPv6) into buf.
// buf must be at least TCPME_ADDRPORTSTRLEN bytes. Returns false on failure.
bool tcpme_get_peer_addr(tcpme_socket_t sock, char *buf, size_t buflen);

// Write the local "IP:port" of sock into buf. Useful for logging the
// address a server is actually bound to.
// buf must be at least TCPME_ADDRPORTSTRLEN bytes. Returns false on failure.
bool tcpme_get_local_addr(tcpme_socket_t sock, char *buf, size_t buflen);

// Write only the IP address (no port) of the peer into buf.
// buf must be at least TCPME_ADDRSTRLEN bytes. Returns false on failure.
bool tcpme_get_peer_ip(tcpme_socket_t sock, char *buf, size_t buflen);

// --- UDP (IPv4 datagram, e.g. for LAN broadcast discovery) ------------------
//
// These are IPv4-only: limited broadcast (255.255.255.255) is an IPv4 concept.
// They provide datagram mechanism only; the caller owns the port and payload.

// Open an IPv4 UDP socket, bound so it can receive immediately. bind_port=0
// binds to a kernel-assigned ephemeral port (read it back with
// tcpme_get_local_addr); a non-zero bind_port binds to INADDR_ANY:port so the
// socket receives datagrams (including broadcasts) sent to that port. If
// broadcast is true, SO_BROADCAST is enabled so the socket may send to a
// broadcast address. Returns TCPME_INVALID_SOCKET on failure.
tcpme_socket_t tcpme_udp_open(uint16_t bind_port, bool broadcast);

// Send len bytes to the IPv4 limited-broadcast address (255.255.255.255) on the
// given port. The socket must have been opened with broadcast=true.
// Returns bytes sent, or -1 on error.
int tcpme_udp_broadcast(tcpme_socket_t sock, uint16_t port, const void *buf, int len);

// Send len bytes to a specific IPv4 address and port (e.g. to unicast a reply
// back to a sender). Returns bytes sent, or -1 on error.
int tcpme_udp_sendto(tcpme_socket_t sock, const char *ip, uint16_t port, const void *buf, int len);

// Receive a datagram into buf (up to len bytes). When non-NULL, out_ip receives
// the sender's numeric IPv4 address (out_iplen must be >= TCPME_ADDRSTRLEN) and
// out_port receives the sender's port. Returns bytes received, or -1 on error.
// Pair with tcpme_check_sockets() for non-blocking readiness polling.
int tcpme_udp_recvfrom(tcpme_socket_t sock, void *buf, int len, char *out_ip, size_t out_iplen,
                       uint16_t *out_port);

// Allocate a socket set for select-based readiness polling.
// capacity is the maximum number of sockets the set will hold.
// Returns NULL on allocation failure.
tcpme_set_t *tcpme_alloc_set(int capacity);

// Free a socket set allocated by tcpme_alloc_set.
void tcpme_free_set(tcpme_set_t *set);

// Add a socket to the set. Returns 0 on success, -1 if full or invalid.
int tcpme_add_socket(tcpme_set_t *set, tcpme_socket_t sock);

// Remove a socket from the set. Returns 0 on success, -1 if not found.
int tcpme_del_socket(tcpme_set_t *set, tcpme_socket_t sock);

// Poll all sockets in the set for readability.
// timeout_ms=0 returns immediately; otherwise blocks up to timeout_ms.
// Returns number of ready sockets, or -1 on error.
int tcpme_check_sockets(tcpme_set_t *set, uint32_t timeout_ms);

// Returns true if sock was readable in the last tcpme_check_sockets call.
bool tcpme_socket_ready(const tcpme_set_t *set, tcpme_socket_t sock);

#ifdef __cplusplus
}
#endif
