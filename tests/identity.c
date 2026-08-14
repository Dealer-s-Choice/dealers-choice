/* Unit tests for the player cryptographic identity (src/dc_identity.c):
 * keypair create/persist/reload, signing, and verification including the
 * tamper-rejection paths. Uses small throwaway key files in the test cwd. */

#include "00_test.h"

#include "dc_identity.h"

#define TMP_KEY "dc_identity_test.key"
#define TMP_KEY2 "dc_identity_test2.key"

static bool is_all_zero(const unsigned char *p, size_t n) {
  for (size_t i = 0; i < n; i++)
    if (p[i])
      return false;
  return true;
}

/* First call mints + persists a keypair; a second call must read back the
 * identical keypair, proving the on-disk roundtrip. */
static void test_create_then_reload(void) {
  remove(TMP_KEY);

  DcIdentity_t created = {0};
  assert(dc_identity_load_or_create(TMP_KEY, &created));
  assert(!is_all_zero(created.public_key, sizeof(created.public_key)));
  assert(!is_all_zero(created.secret_key, sizeof(created.secret_key)));

  DcIdentity_t reloaded = {0};
  assert(dc_identity_load_or_create(TMP_KEY, &reloaded));
  assert(memcmp(created.public_key, reloaded.public_key, sizeof(created.public_key)) == 0);
  assert(memcmp(created.secret_key, reloaded.secret_key, sizeof(created.secret_key)) == 0);

  remove(TMP_KEY);
}

/* A valid signature verifies; any tamper to message, signature, or key fails. */
static void test_sign_and_verify(void) {
  remove(TMP_KEY);
  DcIdentity_t id = {0};
  assert(dc_identity_load_or_create(TMP_KEY, &id));

  const unsigned char msg[] = "challenge-nonce-0123456789";
  size_t len = sizeof(msg) - 1;
  unsigned char sig[crypto_sign_BYTES];
  assert(dc_identity_sign(&id, msg, len, sig));
  assert(dc_identity_verify(id.public_key, msg, len, sig));

  /* Tampered message. */
  unsigned char bad_msg[sizeof(msg)];
  memcpy(bad_msg, msg, sizeof(msg));
  bad_msg[0] ^= 0x01;
  assert(!dc_identity_verify(id.public_key, bad_msg, len, sig));

  /* Tampered signature. */
  unsigned char bad_sig[crypto_sign_BYTES];
  memcpy(bad_sig, sig, sizeof(sig));
  bad_sig[0] ^= 0x01;
  assert(!dc_identity_verify(id.public_key, msg, len, bad_sig));

  /* Wrong identity's public key. */
  remove(TMP_KEY2);
  DcIdentity_t other = {0};
  assert(dc_identity_load_or_create(TMP_KEY2, &other));
  assert(!dc_identity_verify(other.public_key, msg, len, sig));

  remove(TMP_KEY);
  remove(TMP_KEY2);
}

/* A file that exists but is garbage must be rejected, not silently regenerated
 * (regenerating would discard a possibly-recoverable identity). */
static void test_corrupt_file_rejected(void) {
  remove(TMP_KEY);
  FILE *fp = fopen(TMP_KEY, "wb");
  assert(fp);
  const char junk[] = "not a real identity file";
  fwrite(junk, 1, sizeof(junk), fp);
  fclose(fp);

  DcIdentity_t id = {0};
  assert(!dc_identity_load_or_create(TMP_KEY, &id));

  remove(TMP_KEY);
}

_MAIN_HEAD_(void) argc;
(void)argv;
test_create_then_reload();
test_sign_and_verify();
test_corrupt_file_rejected();
fprintf(stderr, "identity tests: OK\n");
_MAIN_TAIL_
