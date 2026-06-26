/*
 net/registry_ledger.c
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

#include "registry_ledger.h"

#include <errno.h>
#include <inttypes.h>
#include <sodium.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "types.h" /* SIZEOF_NICK */
#include "util.h"  /* dc_log, malloc_wrap */

#define PUBKEY_BYTES 32
#define PUBKEY_HEX_LEN (PUBKEY_BYTES * 2) /* 64 chars, no nul */

typedef struct {
  unsigned char pubkey[PUBKEY_BYTES];
  int64_t balance;
  int64_t updated; /* unix seconds of last change */
  char nick[SIZEOF_NICK];
} LedgerEntry_t;

struct RegistryLedger {
  LedgerEntry_t *entries;
  size_t count;
  size_t cap;
  int64_t starting_balance;
};

static int64_t now_seconds(void) {
  return (int64_t)time(NULL);
}

static void set_nick(LedgerEntry_t *e, const char *nick) {
  if (!nick)
    nick = "";
  snprintf(e->nick, sizeof(e->nick), "%s", nick);
  /* The on-disk format is whitespace-delimited with the nick taking the rest of
     the line; a newline would corrupt the next record, so neutralize it. */
  for (char *p = e->nick; *p; p++)
    if (*p == '\n' || *p == '\r')
      *p = ' ';
}

static LedgerEntry_t *find(const RegistryLedger_t *l, const unsigned char pubkey[PUBKEY_BYTES]) {
  for (size_t i = 0; i < l->count; i++)
    if (memcmp(l->entries[i].pubkey, pubkey, PUBKEY_BYTES) == 0)
      return &l->entries[i];
  return NULL;
}

static LedgerEntry_t *add(RegistryLedger_t *l, const unsigned char pubkey[PUBKEY_BYTES],
                          int64_t balance, const char *nick) {
  if (l->count == l->cap) {
    size_t ncap = l->cap ? l->cap * 2 : 16;
    LedgerEntry_t *grown = realloc(l->entries, ncap * sizeof(*grown));
    if (!grown) {
      dc_log(DC_LOG_ERROR, "ledger: out of memory");
      return NULL;
    }
    l->entries = grown;
    l->cap = ncap;
  }
  LedgerEntry_t *e = &l->entries[l->count++];
  memcpy(e->pubkey, pubkey, PUBKEY_BYTES);
  e->balance = balance;
  e->updated = now_seconds();
  set_nick(e, nick);
  return e;
}

RegistryLedger_t *registry_ledger_new(int64_t starting_balance) {
  RegistryLedger_t *l = calloc(1, sizeof(*l));
  if (!l)
    return NULL;
  l->starting_balance = starting_balance;
  return l;
}

void registry_ledger_free(RegistryLedger_t *l) {
  if (!l)
    return;
  free(l->entries);
  free(l);
}

bool registry_ledger_peek(const RegistryLedger_t *l, const unsigned char pubkey[PUBKEY_BYTES],
                          int64_t *out_balance) {
  LedgerEntry_t *e = find(l, pubkey);
  if (!e)
    return false;
  if (out_balance)
    *out_balance = e->balance;
  return true;
}

int64_t registry_ledger_join(RegistryLedger_t *l, const unsigned char pubkey[PUBKEY_BYTES],
                             const char *nick) {
  LedgerEntry_t *e = find(l, pubkey);
  if (!e) {
    e = add(l, pubkey, l->starting_balance, nick);
    return e ? e->balance : l->starting_balance;
  }
  /* Busted balance: top back up so play money stays playable. */
  if (e->balance <= 0)
    e->balance = l->starting_balance;
  e->updated = now_seconds();
  set_nick(e, nick);
  return e->balance;
}

int64_t registry_ledger_apply(RegistryLedger_t *l, const unsigned char pubkey[PUBKEY_BYTES],
                              int64_t delta, const char *nick) {
  LedgerEntry_t *e = find(l, pubkey);
  if (!e) {
    e = add(l, pubkey, l->starting_balance, nick);
    if (!e)
      return l->starting_balance;
  }
  e->balance += delta;
  if (e->balance < 0)
    e->balance = 0;
  e->updated = now_seconds();
  set_nick(e, nick);
  return e->balance;
}

size_t registry_ledger_count(const RegistryLedger_t *l) {
  return l->count;
}

int registry_ledger_load(RegistryLedger_t *l, const char *path) {
  FILE *fp = fopen(path, "r");
  if (!fp) {
    if (errno == ENOENT)
      return 0; /* no ledger yet — fine, start empty */
    dc_log(DC_LOG_ERROR, "ledger: open %s: %s", path, strerror(errno));
    return -1;
  }

  char line[256];
  while (fgets(line, sizeof(line), fp)) {
    char hex[PUBKEY_HEX_LEN + 1];
    long long bal = 0, upd = 0;
    int off = 0;
    /* "<64-hex> <balance> <updated> <nick...>" */
    if (sscanf(line, "%64s %lld %lld %n", hex, &bal, &upd, &off) < 3)
      continue;
    if (strlen(hex) != PUBKEY_HEX_LEN)
      continue;

    unsigned char pubkey[PUBKEY_BYTES];
    size_t bin_len = 0;
    if (sodium_hex2bin(pubkey, sizeof(pubkey), hex, PUBKEY_HEX_LEN, NULL, &bin_len, NULL) != 0 ||
        bin_len != PUBKEY_BYTES)
      continue;

    char *nick = (off > 0) ? line + off : (char *)"";
    nick[strcspn(nick, "\r\n")] = '\0';

    LedgerEntry_t *e = find(l, pubkey);
    if (!e)
      e = add(l, pubkey, (int64_t)bal, nick);
    else {
      e->balance = (int64_t)bal;
      set_nick(e, nick);
    }
    if (e)
      e->updated = (int64_t)upd;
  }

  fclose(fp);
  return 0;
}

int registry_ledger_save(const RegistryLedger_t *l, const char *path) {
  size_t tmp_len = strlen(path) + sizeof(".tmp");
  char *tmp = malloc_wrap(tmp_len);
  snprintf(tmp, tmp_len, "%s.tmp", path);

  int rc = -1;
  FILE *fp = fopen(tmp, "w");
  if (!fp) {
    dc_log(DC_LOG_ERROR, "ledger: open %s: %s", tmp, strerror(errno));
    goto done;
  }

  char hex[PUBKEY_HEX_LEN + 1];
  for (size_t i = 0; i < l->count; i++) {
    const LedgerEntry_t *e = &l->entries[i];
    sodium_bin2hex(hex, sizeof(hex), e->pubkey, PUBKEY_BYTES);
    if (fprintf(fp, "%s %" PRId64 " %" PRId64 " %s\n", hex, e->balance, e->updated, e->nick) < 0) {
      dc_log(DC_LOG_ERROR, "ledger: write %s: %s", tmp, strerror(errno));
      fclose(fp);
      remove(tmp);
      goto done;
    }
  }
  if (fclose(fp) != 0) {
    dc_log(DC_LOG_ERROR, "ledger: close %s: %s", tmp, strerror(errno));
    remove(tmp);
    goto done;
  }
  if (rename(tmp, path) != 0) {
    dc_log(DC_LOG_ERROR, "ledger: rename %s -> %s: %s", tmp, path, strerror(errno));
    remove(tmp);
    goto done;
  }
  rc = 0;

done:
  free(tmp);
  return rc;
}
