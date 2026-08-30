/* Round-trips the on-disk half of reconnect-with-stack (#112): the token file
 * must be scoped per server AND per nick, must survive a save/load cycle, and
 * must degrade to "no token" -- never to a wrong one -- when it is missing,
 * truncated, or cleared.
 *
 * POSIX only: the store prefers XDG_RUNTIME_DIR, which the test can redirect.
 * On Windows it resolves LOCALAPPDATA, which the test could not point away from
 * the user's real profile. */

#include "00_test.h"

#include "reconnect_store.h"

#ifndef _WIN32
#include <sodium.h>

static bool is_zero(const unsigned char t[RECONNECT_TOKEN_LEN]) {
  return sodium_is_zero(t, RECONNECT_TOKEN_LEN) == 1;
}

static void fill(unsigned char t[RECONNECT_TOKEN_LEN], unsigned char v) {
  memset(t, v, RECONNECT_TOKEN_LEN);
}

static void test_roundtrip_and_scoping(void) {
  unsigned char saved[RECONNECT_TOKEN_LEN], got[RECONNECT_TOKEN_LEN];
  unsigned char zeros[RECONNECT_TOKEN_LEN] = {0};
  fill(saved, 0xA7);

  /* Start from a known-empty store: the build dir persists between runs, so a
     token left by the previous run would make the assertion below pass or fail
     depending on run order rather than on behaviour. */
  reconnect_store_save("example.test", 22777, "andy", zeros);
  reconnect_store_save("example.test", 22778, "andy", zeros);
  reconnect_store_save("other.test", 22777, "andy", zeros);
  reconnect_store_save("example.test", 22777, "someone-else", zeros);

  /* Nothing stored yet. */
  reconnect_store_load("example.test", 22777, "andy", got);
  assert(is_zero(got));

  reconnect_store_save("example.test", 22777, "andy", saved);
  reconnect_store_load("example.test", 22777, "andy", got);
  assert(memcmp(saved, got, RECONNECT_TOKEN_LEN) == 0);

  /* A different server must not hand back this server's token... */
  reconnect_store_load("example.test", 22778, "andy", got);
  assert(is_zero(got));
  reconnect_store_load("other.test", 22777, "andy", got);
  assert(is_zero(got));

  /* ...nor a different player on the same server. Several clients on one
     machine sharing a config dir would otherwise collide. */
  reconnect_store_load("example.test", 22777, "someone-else", got);
  assert(is_zero(got));
}

static void test_zero_token_clears(void) {
  unsigned char saved[RECONNECT_TOKEN_LEN], got[RECONNECT_TOKEN_LEN];
  fill(saved, 0x5C);

  reconnect_store_save("clear.test", 22777, "andy", saved);
  reconnect_store_load("clear.test", 22777, "andy", got);
  assert(memcmp(saved, got, RECONNECT_TOKEN_LEN) == 0);

  unsigned char cleared[RECONNECT_TOKEN_LEN] = {0};
  reconnect_store_save("clear.test", 22777, "andy", cleared);
  reconnect_store_load("clear.test", 22777, "andy", got);
  assert(is_zero(got));
}

static void test_saving_twice_replaces(void) {
  unsigned char first[RECONNECT_TOKEN_LEN], second[RECONNECT_TOKEN_LEN],
      got[RECONNECT_TOKEN_LEN];
  fill(first, 0x11);
  fill(second, 0x22);

  /* The server mints a fresh token every join, so overwriting is the common
     path, not an edge case -- and O_EXCL on the temp file must not break it. */
  reconnect_store_save("replace.test", 22777, "andy", first);
  reconnect_store_save("replace.test", 22777, "andy", second);
  reconnect_store_load("replace.test", 22777, "andy", got);
  assert(memcmp(second, got, RECONNECT_TOKEN_LEN) == 0);
}
#endif /* !_WIN32 */

_MAIN_HEAD_(void) argc;
(void)argv;
#ifdef _WIN32
fprintf(stderr, "reconnect-store tests: skipped (no redirectable config dir)\n");
#else
{
  /* Redirect the store away from the real one. XDG_RUNTIME_DIR is what
     token_dir() prefers, so setting it covers the path actually taken;
     MESON_BUILD_TEST_ROOT is set for every test by tests/meson.build. */
  const char *root = getenv("MESON_BUILD_TEST_ROOT");
  char dir[1024];
  snprintf(dir, sizeof dir, "%s/reconnect_store_state", root ? root : ".");
  assert(setenv("XDG_RUNTIME_DIR", dir, 1) == 0);
}
test_roundtrip_and_scoping();
test_zero_token_clears();
test_saving_twice_replaces();
fprintf(stderr, "reconnect-store tests: OK\n");
#endif
_MAIN_TAIL_
