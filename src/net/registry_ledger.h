/*
 net/registry_ledger.h
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

#ifndef DC_REGISTRY_LEDGER_H
#define DC_REGISTRY_LEDGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 The central chip-balance ledger held by the registry (#67). It maps a player's
 ed25519 public key (their portable identity) to a persistent balance, so a
 player has ONE balance across every trusted server rather than a separate stash
 per server. The registry — not any game server — owns this state; trusted
 servers read a balance at join and report a signed delta at hand end.

 Keys are the raw 32-byte public key. The starting balance (also the bust-reset
 value) is configured once when the ledger is created.
*/
typedef struct RegistryLedger RegistryLedger_t;

/* Create an empty ledger. starting_balance seeds new identities and is the value
   a busted (<= 0) balance resets to at join. Returns NULL on allocation failure. */
RegistryLedger_t *registry_ledger_new(int64_t starting_balance);
void registry_ledger_free(RegistryLedger_t *ledger);

/* Look up a balance without mutating the ledger. Returns true and sets *out_balance
   if the identity is known; false if it has never been seen. */
bool registry_ledger_peek(const RegistryLedger_t *ledger, const unsigned char pubkey[32],
                          int64_t *out_balance);

/* Balance to hand a player when they JOIN a table. Seeds starting_balance for an
   unknown identity, and resets a busted (<= 0) balance back to starting_balance so
   play money stays playable. Records the seed/reset. Returns the balance to use. */
int64_t registry_ledger_join(RegistryLedger_t *ledger, const unsigned char pubkey[32],
                             const char *nick);

/* Apply a signed delta to a balance (seeding starting_balance first if the
   identity is unknown), flooring the result at 0. Updates the recorded nick and
   timestamp. Returns the new balance. This is what a trusted server's hand-end
   report drives; deltas compose correctly even if the player is active on two
   servers at once. */
int64_t registry_ledger_apply(RegistryLedger_t *ledger, const unsigned char pubkey[32],
                              int64_t delta, const char *nick);

/* Load entries from a file written by registry_ledger_save. A missing file is not
   an error (the ledger is simply left empty). Returns 0 on success, -1 on a read
   error; malformed individual lines are skipped. */
int registry_ledger_load(RegistryLedger_t *ledger, const char *path);

/* Write the ledger to path atomically (temp file + rename). Returns 0 on success. */
int registry_ledger_save(const RegistryLedger_t *ledger, const char *path);

/* Number of identities currently tracked. */
size_t registry_ledger_count(const RegistryLedger_t *ledger);

#endif /* DC_REGISTRY_LEDGER_H */
