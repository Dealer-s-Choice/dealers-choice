/*
 fuzz_util.h
 https://github.com/Dealer-s-Choice/dealers-choice

 MIT License

 Copyright (c) 2026 Andy Alt

 Initial version written by Claude (Opus 4.8, an LLM by Anthropic) at Andy's
 direction.
*/

/*
 * Shared helpers for the wire-parser fuzz harnesses (test_net_fuzz,
 * test_registry_fuzz, test_lan_fuzz). These hammer the three internet-facing
 * parsers with random + mutated bytes under ASan/UBSan: the point is that the
 * sanitized build aborts on any out-of-bounds read/write or undefined behavior
 * the parsers might commit on hostile input, so a malformed datagram can't
 * crash the live registry/server/client.
 *
 * A self-contained PCG32 (the same minimal-PCG algorithm as subprojects/pcg)
 * is embedded here rather than reused from deckhandler so the byte generator
 * is deterministic across seeds without depending on which pcg variant the
 * host installs, and so it carries its own local state (no clash with the
 * global `rng`).
 */

#ifndef DC_FUZZ_UTIL_H
#define DC_FUZZ_UTIL_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* --- Minimal PCG32 (O'Neill's pcg_basic, public-domain algorithm) --- */

typedef struct {
  uint64_t state;
  uint64_t inc;
} fuzz_rng_t;

static inline void fuzz_srand(fuzz_rng_t *r, uint64_t seed, uint64_t seq) {
  r->state = 0u;
  r->inc = (seq << 1u) | 1u;
  /* prime the generator (matches pcg32_srandom_r) */
  r->state = r->state * 6364136223846793005ULL + r->inc;
  r->state += seed;
  r->state = r->state * 6364136223846793005ULL + r->inc;
}

static inline uint32_t fuzz_rand(fuzz_rng_t *r) {
  uint64_t old = r->state;
  r->state = old * 6364136223846793005ULL + r->inc;
  uint32_t xorshifted = (uint32_t)(((old >> 18u) ^ old) >> 27u);
  uint32_t rot = (uint32_t)(old >> 59u);
  return (xorshifted >> rot) | (xorshifted << ((-rot) & 31u));
}

/* Uniform in [0, bound) without modulo bias. bound==0 returns 0. */
static inline uint32_t fuzz_bounded(fuzz_rng_t *r, uint32_t bound) {
  if (bound == 0)
    return 0;
  uint32_t threshold = (uint32_t)(-bound) % bound;
  for (;;) {
    uint32_t v = fuzz_rand(r);
    if (v >= threshold)
      return v % bound;
  }
}

static inline uint8_t fuzz_byte(fuzz_rng_t *r) { return (uint8_t)(fuzz_rand(r) & 0xff); }

/* Fill buf with `len` random bytes. */
static inline void fuzz_fill(fuzz_rng_t *r, uint8_t *buf, size_t len) {
  for (size_t i = 0; i < len; i++)
    buf[i] = fuzz_byte(r);
}

/*
 * Mutate a valid message in place: copy `src` into `dst` (capacity `cap`),
 * apply a random mutation, and return the resulting length. Mutations model a
 * hostile peer corrupting an otherwise-well-formed message: byte flips,
 * truncation, zero-extension (oversize), and single-byte injection. The
 * returned length is always <= cap.
 */
static inline size_t fuzz_mutate(fuzz_rng_t *r, const uint8_t *src, size_t src_len, uint8_t *dst,
                                 size_t cap) {
  size_t len = src_len < cap ? src_len : cap;
  memcpy(dst, src, len);

  switch (fuzz_bounded(r, 5)) {
  case 0: { /* flip a handful of random bytes */
    if (len == 0)
      break;
    uint32_t flips = 1 + fuzz_bounded(r, 8);
    for (uint32_t i = 0; i < flips; i++)
      dst[fuzz_bounded(r, (uint32_t)len)] ^= fuzz_byte(r);
    break;
  }
  case 1: /* truncate */
    if (len > 0)
      len = fuzz_bounded(r, (uint32_t)len);
    break;
  case 2: { /* oversize: append random bytes up to cap */
    size_t extra = fuzz_bounded(r, (uint32_t)(cap - len) + 1);
    fuzz_fill(r, dst + len, extra);
    len += extra;
    break;
  }
  case 3: /* set one byte to a random value */
    if (len > 0)
      dst[fuzz_bounded(r, (uint32_t)len)] = fuzz_byte(r);
    break;
  case 4: /* zero one byte (common length/field-clearing corruption) */
    if (len > 0)
      dst[fuzz_bounded(r, (uint32_t)len)] = 0;
    break;
  }
  return len;
}

#endif /* DC_FUZZ_UTIL_H */
