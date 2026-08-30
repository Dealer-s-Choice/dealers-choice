/*
 reconnect_store.c
 https://github.com/Dealer-s-Choice/dealers_choice

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

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32
#include <io.h>
#include <process.h>
#define dc_open _open
#define dc_write _write
#define dc_close _close
#define dc_getpid _getpid
#define DC_OPEN_FLAGS (_O_WRONLY | _O_CREAT | _O_EXCL | _O_BINARY)
#define DC_OPEN_MODE (_S_IREAD | _S_IWRITE)
#else
#include <unistd.h>
#define dc_open open
#define dc_write write
#define dc_close close
#define dc_getpid getpid
#define DC_OPEN_FLAGS (O_WRONLY | O_CREAT | O_EXCL)
#define DC_OPEN_MODE (S_IRUSR | S_IWUSR)
#endif

#include <canfigger.h>
#include <sodium.h>

#include "config.h"
#include "reconnect_store.h"
#include "util.h"

/* Where a session token belongs.

   NOT the config dir: this is neither configuration nor durable data, it is a
   secret with a two-minute life, and people keep ~/.config in dotfile repos and
   backups. canfigger_cache_dir matches the semantics instead -- XDG cache is
   explicitly deletable at any time without loss, which is true here (losing the
   token costs a reclaim, never a join) -- and it resolves CSIDL_LOCAL_APPDATA on
   Windows rather than the roaming APPDATA that canfigger_config_dir uses. A
   machine-local token must not be synced to the user's other machines.

   XDG_RUNTIME_DIR is preferred where the session provides one: it is user-only
   and cleared at logout, so a stale credential does not outlive the login. That
   is the one location canfigger has no helper for; everything else, including
   all of Windows, comes from the library. */
static char *token_dir(void) {
#ifndef _WIN32
  const char *run = getenv("XDG_RUNTIME_DIR");
  if (run && *run)
    return canfigger_path_join(run, DEALERSCHOICE_NAME);
#endif
  return canfigger_cache_dir(DEALERSCHOICE_NAME);
}

/* The filename is a hash of server+nick rather than those values verbatim: a
   host string comes off the network-ish side of the config and must never be
   able to steer a path ("..", a separator, a device name on Windows). Hashing
   also keeps the name a fixed, filesystem-safe length. */
static char *token_pathname(const char *host, uint16_t port, const char *nick) {
  char key[512];
  snprintf(key, sizeof key, "%s|%u|%s", host ? host : "", (unsigned)port, nick ? nick : "");

  unsigned char digest[16];
  crypto_generichash(digest, sizeof digest, (const unsigned char *)key, strlen(key), NULL, 0);

  char name[sizeof(digest) * 2 + sizeof(".session")];
  for (size_t i = 0; i < sizeof digest; i++)
    snprintf(name + i * 2, 3, "%02x", digest[i]);
  memcpy(name + sizeof(digest) * 2, ".session", sizeof(".session"));

  char *dir = token_dir();
  if (!dir)
    return NULL;
  char *path = canfigger_path_join(dir, name);
  free(dir);
  return path;
}

void reconnect_store_load(const char *host, uint16_t port, const char *nick,
                          unsigned char out[RECONNECT_TOKEN_LEN]) {
  memset(out, 0, RECONNECT_TOKEN_LEN);

  char *path = token_pathname(host, port, nick);
  if (!path)
    return;

  FILE *fp = fopen(path, "rb");
  if (!fp) {
    /* No stored token is the normal first-run case, not a problem to report. */
    free(path);
    return;
  }

  unsigned char buf[RECONNECT_TOKEN_LEN];
  const bool ok = fread(buf, 1, sizeof buf, fp) == sizeof buf && fgetc(fp) == EOF;
  fclose(fp);

  if (ok)
    memcpy(out, buf, RECONNECT_TOKEN_LEN);
  else
    dc_log(DC_LOG_DEBUG, "reconnect: ignoring malformed token file %s", path);

  sodium_memzero(buf, sizeof buf);
  free(path);
}

void reconnect_store_save(const char *host, uint16_t port, const char *nick,
                          const unsigned char token[RECONNECT_TOKEN_LEN]) {
  char *path = token_pathname(host, port, nick);
  if (!path)
    return;

  if (sodium_is_zero(token, RECONNECT_TOKEN_LEN) == 1) {
    remove(path);
    free(path);
    return;
  }

  /* pid in the temp name, plus O_EXCL: two clients saving at once must not
     write through each other's file the way a shared "<path>.tmp" would. */
  size_t tmp_len = strlen(path) + 32;
  char *tmp = malloc(tmp_len);
  if (!tmp) {
    free(path);
    return;
  }
  snprintf(tmp, tmp_len, "%s.%ld.tmp", path, (long)dc_getpid());

  int fd = dc_open(tmp, DC_OPEN_FLAGS, DC_OPEN_MODE);
  if (fd < 0 && errno == ENOENT) {
    /* First write of the session: the directory may not exist yet -- the
       runtime and state dirs get no per-app subdirectory made for us. */
    char *dir = token_dir();
    if (dir) {
      make_directory_recursive(dir);
      free(dir);
    }
    fd = dc_open(tmp, DC_OPEN_FLAGS, DC_OPEN_MODE);
  }
  if (fd < 0) {
    dc_log(DC_LOG_DEBUG, "reconnect: open %s: %s", tmp, strerror(errno));
    goto done;
  }
  if (dc_write(fd, token, RECONNECT_TOKEN_LEN) != (int)RECONNECT_TOKEN_LEN) {
    dc_log(DC_LOG_DEBUG, "reconnect: write %s: %s", tmp, strerror(errno));
    dc_close(fd);
    remove(tmp);
    goto done;
  }
  dc_close(fd);

#ifdef _WIN32
  /* rename() will not replace an existing file on Windows. */
  remove(path);
#endif
  if (rename(tmp, path) != 0) {
    dc_log(DC_LOG_DEBUG, "reconnect: rename %s: %s", tmp, strerror(errno));
    remove(tmp);
  }

done:
  free(tmp);
  free(path);
}
