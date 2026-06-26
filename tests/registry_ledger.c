/* Unit tests for the registry chip-balance ledger (src/net/registry_ledger.c):
 * join-seeding, busted-balance reset, delta application with a zero floor,
 * peek, and the save/load roundtrip. Pure in-memory + a throwaway file; no
 * sockets or real keypairs (the ledger keys on raw 32-byte values). */

#include "00_test.h"

#include "net/registry_ledger.h"

#define START 20000
#define LEDGER_FILE "dc_ledger_test.dat"

static void fill_key(unsigned char k[32], unsigned char byte) {
  memset(k, byte, 32);
}

static void test_join_seeds_and_resets(void) {
  RegistryLedger_t *l = registry_ledger_new(START);
  assert(l);

  unsigned char a[32];
  fill_key(a, 0xA1);

  /* Unknown identity is seeded to the starting balance. */
  assert(registry_ledger_join(l, a, "Alice") == START);
  assert(registry_ledger_count(l) == 1);

  /* Lose it all, then a rejoin tops back up to the starting balance. */
  assert(registry_ledger_apply(l, a, -START, "Alice") == 0);
  assert(registry_ledger_join(l, a, "Alice") == START);
  assert(registry_ledger_count(l) == 1); /* still one identity */

  registry_ledger_free(l);
}

static void test_apply_delta_and_floor(void) {
  RegistryLedger_t *l = registry_ledger_new(START);
  unsigned char a[32];
  fill_key(a, 0xB2);

  assert(registry_ledger_join(l, a, "Bob") == START);
  assert(registry_ledger_apply(l, a, 5000, "Bob") == START + 5000);
  assert(registry_ledger_apply(l, a, -1000, "Bob") == START + 4000);
  /* A delta past zero floors at zero, not negative. */
  assert(registry_ledger_apply(l, a, -999999, "Bob") == 0);

  /* apply on an unknown identity seeds starting first, then applies. */
  unsigned char c[32];
  fill_key(c, 0xC3);
  assert(registry_ledger_apply(l, c, 1000, "Carol") == START + 1000);

  registry_ledger_free(l);
}

static void test_peek(void) {
  RegistryLedger_t *l = registry_ledger_new(START);
  unsigned char a[32], b[32];
  fill_key(a, 0x11);
  fill_key(b, 0x22);

  int64_t bal = -1;
  assert(!registry_ledger_peek(l, a, &bal)); /* unknown */
  registry_ledger_join(l, a, "A");
  assert(registry_ledger_peek(l, a, &bal) && bal == START);
  assert(!registry_ledger_peek(l, b, &bal)); /* still unknown */

  registry_ledger_free(l);
}

static void test_save_load_roundtrip(void) {
  remove(LEDGER_FILE);
  unsigned char a[32], b[32];
  fill_key(a, 0xAA);
  fill_key(b, 0xBB);

  RegistryLedger_t *l = registry_ledger_new(START);
  registry_ledger_join(l, a, "Alice");
  registry_ledger_apply(l, a, 1234, "Alice");      /* 21234 */
  registry_ledger_apply(l, b, -5000, "Bob Spaces"); /* seed 20000 -> 15000, nick with a space */
  assert(registry_ledger_save(l, LEDGER_FILE) == 0);
  registry_ledger_free(l);

  RegistryLedger_t *l2 = registry_ledger_new(START);
  assert(registry_ledger_load(l2, LEDGER_FILE) == 0);
  assert(registry_ledger_count(l2) == 2);
  int64_t bal = 0;
  assert(registry_ledger_peek(l2, a, &bal) && bal == START + 1234);
  assert(registry_ledger_peek(l2, b, &bal) && bal == START - 5000);
  registry_ledger_free(l2);

  /* Loading a missing file is not an error; the ledger stays empty. */
  remove(LEDGER_FILE);
  RegistryLedger_t *l3 = registry_ledger_new(START);
  assert(registry_ledger_load(l3, LEDGER_FILE) == 0);
  assert(registry_ledger_count(l3) == 0);
  registry_ledger_free(l3);
}

_MAIN_HEAD_(void) argc;
(void)argv;
test_join_seeds_and_resets();
test_apply_delta_and_floor();
test_peek();
test_save_load_roundtrip();
fprintf(stderr, "registry_ledger tests: OK\n");
_MAIN_TAIL_
