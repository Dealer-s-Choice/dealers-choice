/*
 hotkey_table.h
 https://github.com/Dealer-s-Choice/dealers_choice

 Written by Claude (Anthropic, Opus 4.8) at Andy's direction.

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

/*
 * Single source of truth for the game-play hotkey list.
 *
 * Two consumers read these tables and must never drift apart:
 *   1. The in-game F1 reference overlay (src/ui/game_logic.c), which shows the
 *      player their current bindings without leaving the table.
 *   2. The docs generator (tools/gen_keys_doc.c), which writes docs/keys.md at
 *      build/dist time.
 *
 * The tables are deliberately SDL-free (plain strings) so the tiny doc-gen tool
 * can include them without linking SDL.  The configurable defaults here mirror
 * the HK(...) rows in src/dc_config.c's player_config_entries[]; if you change a
 * default there, change it here too (there is no compile-time link between the
 * two, only this comment).
 */

#ifndef __HOTKEY_TABLE_H
#define __HOTKEY_TABLE_H

#include <stdbool.h>
#include <stddef.h>

/* A configurable action: which player.conf key sets it, its built-in default,
 * and a short human description.  config_key is the player.conf entry name
 * (e.g. "hotkey_check"); strip the "hotkey_" prefix for the action label. */
typedef struct {
  const char *config_key;  /* player.conf key, e.g. "hotkey_check"          */
  const char *default_key; /* default key name when unset, e.g. "c"         */
  const char *description; /* plain-English description (marked with N_)     */
} HotkeyDef_t;

/* A fixed, non-configurable key (or key range): what to press and what it does.
 * These are listed in the overlay and the docs so players see the full picture,
 * but the hotkey editor refuses to bind them (see is_reserved_hotkey). */
typedef struct {
  const char *keys;        /* key label, e.g. "1-8" or "Esc"                */
  const char *description; /* plain-English description (marked with N_)     */
  bool in_game_only;       /* true => only meaningful during a hand, so the  */
                           /* F1 overlay hides it on the connect screen /    */
                           /* lobby.  keys.md always lists every fixed key.  */
} FixedKeyDef_t;

extern const HotkeyDef_t g_hotkey_defs[];
extern const size_t g_hotkey_def_count;

extern const FixedKeyDef_t g_fixed_keys[];
extern const size_t g_fixed_key_count;

#endif
