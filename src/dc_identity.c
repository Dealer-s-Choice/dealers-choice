/*
 dc_identity.c
 https://github.com/Dealer-s-Choice/dealers_choice

 MIT License

 Copyright (c) 2026 Andy Alt
 This file was written by Claude (Opus 4.8) at Andy's direction.

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

#include "dc_identity.h"

#include <canfigger.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <sys/stat.h> /* chmod */
#endif

#include "config.h" /* DEALERSCHOICE_NAME */
#include "util.h"   /* dc_log, check_pathname_state, make_directory_recursive */

/*
 On-disk layout (104 bytes). The magic + version byte let a later format (e.g.
 the recovery-phrase scheme) be told apart from this one without guessing.
*/
#define DC_ID_MAGIC "DCID"
#define DC_ID_MAGIC_LEN 4
#define DC_ID_VERSION 1
#define DC_ID_HEADER_LEN 8 /* magic[4] + version[1] + reserved[3] */
#define DC_ID_FILE_LEN                                                                              \
  (DC_ID_HEADER_LEN + crypto_sign_PUBLICKEYBYTES + crypto_sign_SECRETKEYBYTES)

/* Player_t stores the public key as a plain 32-byte array (types.h, which must
   stay free of the sodium header). Keep that literal honest. */
_Static_assert(crypto_sign_PUBLICKEYBYTES == 32, "ed25519 public key must be 32 bytes");

/* Create the directory that will contain `path`, if it does not already exist. */
static bool ensure_parent_dir(const char *path) {
  const char *slash = strrchr(path, '/');
#ifdef _WIN32
  const char *bslash = strrchr(path, '\\');
  if (bslash && (!slash || bslash > slash))
    slash = bslash;
#endif
  if (!slash || slash == path)
    return true; /* no directory component, or root */

  size_t dir_len = (size_t)(slash - path);
  char *dir = malloc_wrap(dir_len + 1);
  memcpy(dir, path, dir_len);
  dir[dir_len] = '\0';

  bool ok = true;
  EPathState state = check_pathname_state(dir);
  if (state == PATH_NOT_FOUND)
    ok = (make_directory_recursive(dir) == 0);
  else if (state == PATH_ERROR)
    ok = false;
  if (!ok)
    dc_log(DC_LOG_ERROR, "identity: cannot create dir %s", dir);
  free(dir);
  return ok;
}

static bool read_identity(const char *path, DcIdentity_t *out) {
  FILE *fp = fopen(path, "rb");
  if (!fp) {
    dc_log(DC_LOG_ERROR, "identity: open %s: %s", path, strerror(errno));
    return false;
  }

  unsigned char buf[DC_ID_FILE_LEN];
  size_t n = fread(buf, 1, sizeof(buf), fp);
  fclose(fp);
  if (n != sizeof(buf)) {
    dc_log(DC_LOG_ERROR, "identity: %s is truncated or unreadable", path);
    return false;
  }
  if (memcmp(buf, DC_ID_MAGIC, DC_ID_MAGIC_LEN) != 0 || buf[DC_ID_MAGIC_LEN] != DC_ID_VERSION) {
    dc_log(DC_LOG_ERROR, "identity: %s has an unknown format", path);
    return false;
  }

  const unsigned char *pk = buf + DC_ID_HEADER_LEN;
  const unsigned char *sk = pk + crypto_sign_PUBLICKEYBYTES;
  memcpy(out->public_key, pk, crypto_sign_PUBLICKEYBYTES);
  memcpy(out->secret_key, sk, crypto_sign_SECRETKEYBYTES);

  /* Integrity check: the public key must match the one derived from the secret
     key. A mismatch means a corrupt/tampered file — fail rather than silently
     regenerate, which would discard a possibly-recoverable identity. */
  unsigned char derived[crypto_sign_PUBLICKEYBYTES];
  if (crypto_sign_ed25519_sk_to_pk(derived, out->secret_key) != 0 ||
      memcmp(derived, out->public_key, crypto_sign_PUBLICKEYBYTES) != 0) {
    dc_log(DC_LOG_ERROR, "identity: %s is corrupt (key mismatch)", path);
    return false;
  }
  return true;
}

static bool write_identity(const char *path, const DcIdentity_t *id) {
  size_t tmp_len = strlen(path) + sizeof(".tmp");
  char *tmp = malloc_wrap(tmp_len);
  snprintf(tmp, tmp_len, "%s.tmp", path);

  unsigned char buf[DC_ID_FILE_LEN];
  memset(buf, 0, sizeof(buf));
  memcpy(buf, DC_ID_MAGIC, DC_ID_MAGIC_LEN);
  buf[DC_ID_MAGIC_LEN] = DC_ID_VERSION;
  memcpy(buf + DC_ID_HEADER_LEN, id->public_key, crypto_sign_PUBLICKEYBYTES);
  memcpy(buf + DC_ID_HEADER_LEN + crypto_sign_PUBLICKEYBYTES, id->secret_key,
         crypto_sign_SECRETKEYBYTES);

  bool ok = false;
  FILE *fp = fopen(tmp, "wb");
  if (!fp) {
    dc_log(DC_LOG_ERROR, "identity: open %s: %s", tmp, strerror(errno));
    goto done;
  }
  if (fwrite(buf, 1, sizeof(buf), fp) != sizeof(buf)) {
    dc_log(DC_LOG_ERROR, "identity: write %s: %s", tmp, strerror(errno));
    fclose(fp);
    remove(tmp);
    goto done;
  }
  if (fclose(fp) != 0) {
    dc_log(DC_LOG_ERROR, "identity: close %s: %s", tmp, strerror(errno));
    remove(tmp);
    goto done;
  }
#ifndef _WIN32
  /* The secret key is sensitive: owner-only. (Windows uses ACLs; the per-user
     profile dir is already private, so we leave it to the default there.) */
  if (chmod(tmp, S_IRUSR | S_IWUSR) != 0)
    dc_log(DC_LOG_ERROR, "identity: chmod %s: %s", tmp, strerror(errno));
#endif
  if (rename(tmp, path) != 0) {
    dc_log(DC_LOG_ERROR, "identity: rename %s -> %s: %s", tmp, path, strerror(errno));
    remove(tmp);
    goto done;
  }
  ok = true;

done:
  sodium_memzero(buf, sizeof(buf));
  free(tmp);
  return ok;
}

bool dc_identity_load_or_create(const char *path, DcIdentity_t *out) {
  if (!path || !out)
    return false;
  if (sodium_init() < 0) {
    dc_log(DC_LOG_ERROR, "identity: sodium_init failed");
    return false;
  }

  /* Probe for the file by opening it. (check_pathname_state only recognizes
     directories, so it is the wrong tool for a regular file.) */
  FILE *probe = fopen(path, "rb");
  if (probe) {
    fclose(probe);
    return read_identity(path, out);
  }
  if (errno != ENOENT) {
    dc_log(DC_LOG_ERROR, "identity: cannot access %s: %s", path, strerror(errno));
    return false;
  }

  /* Not found: mint a new identity and persist it. */
  if (!ensure_parent_dir(path))
    return false;
  if (crypto_sign_keypair(out->public_key, out->secret_key) != 0) {
    dc_log(DC_LOG_ERROR, "identity: keypair generation failed");
    return false;
  }
  if (!write_identity(path, out)) {
    sodium_memzero(out, sizeof(*out));
    return false;
  }
  return true;
}

char *dc_identity_default_path(void) {
  const char *override = getenv("DC_IDENTITY_FILE");
  if (override && *override)
    return dc_strdup(override);

  char *dir = canfigger_data_dir(DEALERSCHOICE_NAME);
  if (!dir) {
    dc_log(DC_LOG_ERROR, "identity: cannot determine data directory");
    return NULL;
  }
  char *path = canfigger_path_join(dir, "identity.key");
  free(dir);
  if (!path)
    dc_log(DC_LOG_ERROR, "identity: path join failed");
  return path;
}

bool dc_identity_sign(const DcIdentity_t *id, const unsigned char *msg, size_t len,
                      unsigned char sig[crypto_sign_BYTES]) {
  if (!id || (!msg && len) || !sig)
    return false;
  return crypto_sign_detached(sig, NULL, msg, (unsigned long long)len, id->secret_key) == 0;
}

bool dc_identity_verify(const unsigned char public_key[crypto_sign_PUBLICKEYBYTES],
                        const unsigned char *msg, size_t len,
                        const unsigned char sig[crypto_sign_BYTES]) {
  if (!public_key || (!msg && len) || !sig)
    return false;
  return crypto_sign_verify_detached(sig, msg, (unsigned long long)len, public_key) == 0;
}

const DcIdentity_t *dc_identity_get(void) {
  static DcIdentity_t identity;
  static bool loaded = false;
  if (loaded)
    return &identity;

  char *path = dc_identity_default_path();
  if (!path)
    return NULL;
  bool ok = dc_identity_load_or_create(path, &identity);
  free(path);
  if (!ok)
    return NULL;
  loaded = true;
  return &identity;
}
