/*
 dc_identity.h
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

#ifndef DC_IDENTITY_H
#define DC_IDENTITY_H

#include <sodium.h>
#include <stdbool.h>
#include <stddef.h>

/*
 A player's cryptographic identity: a libsodium ed25519 keypair.

 The PUBLIC key is the portable identity — it is the same on every server and
 the registry, with no central account server, and a server keys a player's
 persistent state (e.g. chip balance, #67) on it. The SECRET key never leaves
 the client; it is used only to sign a server-issued nonce (challenge-response),
 so nothing replayable crosses the wire and transport encryption (#62) stays
 optional rather than required.

 v1 is keyfile-only: the identity lives in one local file, so it is tied to the
 machine that holds it. A recovery-phrase scheme (seed the keypair from a written
 mnemonic, so the same identity can be restored on another device) is the planned
 fast-follow; the on-disk format carries a version byte to allow for it.
*/
typedef struct {
  unsigned char public_key[crypto_sign_PUBLICKEYBYTES]; /* 32 bytes, the identity */
  unsigned char secret_key[crypto_sign_SECRETKEYBYTES]; /* 64 bytes, never sent   */
} DcIdentity_t;

/*
 Load the identity from `path`, or create and persist a new one if `path` does
 not exist. The containing directory is created if needed; the file is written
 atomically and, on POSIX, with 0600 permissions. Calls sodium_init() internally
 (idempotent). Returns true on success (out is filled), false on any error.
*/
bool dc_identity_load_or_create(const char *path, DcIdentity_t *out);

/*
 Resolve the default identity-file path (caller frees with free()). Uses the XDG
 data dir via canfigger — e.g. ~/.local/share/dealers-choice/identity.key on
 Linux, the platform equivalent elsewhere. Returns NULL on error. SDL-free, so
 it is usable from the headless bot and server as well as the GUI client.
*/
char *dc_identity_default_path(void);

/*
 Sign `len` bytes of `msg` with the identity's secret key, writing a detached
 signature into `sig`. Returns true on success.
*/
bool dc_identity_sign(const DcIdentity_t *id, const unsigned char *msg, size_t len,
                      unsigned char sig[crypto_sign_BYTES]);

/*
 Verify a detached signature `sig` over `len` bytes of `msg` against
 `public_key`. Returns true only if the signature is valid.
*/
bool dc_identity_verify(const unsigned char public_key[crypto_sign_PUBLICKEYBYTES],
                        const unsigned char *msg, size_t len,
                        const unsigned char sig[crypto_sign_BYTES]);

#endif /* DC_IDENTITY_H */
