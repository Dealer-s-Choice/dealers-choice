/*
 * test_bytes.c — tcpme_put_be16/32 and tcpme_get_be16/32.
 *
 * These are the framing primitives every protocol header goes through
 * (DCPROTO magic/version/length), so their byte order is pinned here
 * against explicit big-endian byte patterns, not just round-tripped —
 * a round trip alone would pass even if both sides were little-endian.
 */

#include "tcpme_test_helpers.h"

#include "tcpme.h"

int main(void) {
  uint8_t b[4];

  /* --- be16: exact wire bytes --- */
  tcpme_put_be16(b, 0x1234);
  assert(b[0] == 0x12 && b[1] == 0x34);
  assert(tcpme_get_be16(b) == 0x1234);

  /* Endianness-asymmetric value: catches byte-swapped implementations. */
  tcpme_put_be16(b, 0xFF00);
  assert(b[0] == 0xFF && b[1] == 0x00);
  assert(tcpme_get_be16(b) == 0xFF00);

  /* Extremes. */
  tcpme_put_be16(b, 0);
  assert(b[0] == 0 && b[1] == 0);
  assert(tcpme_get_be16(b) == 0);
  tcpme_put_be16(b, 0xFFFF);
  assert(tcpme_get_be16(b) == 0xFFFF);

  /* --- be32: exact wire bytes --- */
  tcpme_put_be32(b, 0x12345678);
  assert(b[0] == 0x12 && b[1] == 0x34 && b[2] == 0x56 && b[3] == 0x78);
  assert(tcpme_get_be32(b) == 0x12345678);

  tcpme_put_be32(b, 0xFF000000);
  assert(b[0] == 0xFF && b[1] == 0x00 && b[2] == 0x00 && b[3] == 0x00);
  assert(tcpme_get_be32(b) == 0xFF000000);

  tcpme_put_be32(b, 0);
  assert(tcpme_get_be32(b) == 0);
  tcpme_put_be32(b, 0xFFFFFFFF);
  assert(tcpme_get_be32(b) == 0xFFFFFFFF);

  /* get on a hand-built buffer (no prior put): parses raw peer bytes. */
  const uint8_t wire[4] = {0xDE, 0xAD, 0xBE, 0xEF};
  assert(tcpme_get_be16(wire) == 0xDEAD);
  assert(tcpme_get_be32(wire) == 0xDEADBEEF);

  return 0;
}
