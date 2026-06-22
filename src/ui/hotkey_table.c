/*
 hotkey_table.c
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

#include "hotkey_table.h"

#include "translate.h" /* N_() marks descriptions for gettext extraction */

/* Configurable game-play actions.  The order here drives the row order in the
 * F1 overlay and in keys.md.  Defaults mirror the HK(...) rows in
 * src/dc_config.c player_config_entries[] — keep the two in sync. */
const HotkeyDef_t g_hotkey_defs[] = {
    {"hotkey_check", "c", N_("Check (pass without betting)")},
    {"hotkey_bet", "b", N_("Bet (open the betting)")},
    {"hotkey_fold", "f", N_("Fold (give up the hand)")},
    {"hotkey_call", "l", N_("Call (match the current bet)")},
    {"hotkey_raise", "r", N_("Raise (increase the bet)")},
    {"hotkey_complete", "o", N_("Complete (finish a partial bet)")},
    {"hotkey_discard", "d", N_("Discard (draw new cards)")},
    {"hotkey_hand_rank", "h", N_("Show your current best hand")},
};

const size_t g_hotkey_def_count = sizeof(g_hotkey_defs) / sizeof(g_hotkey_defs[0]);

/* Fixed keys the player cannot rebind.  Listed so the overlay and the docs show
 * the complete in-game key map, not just the configurable part. */
const FixedKeyDef_t g_fixed_keys[] = {
    {"1-5", N_("Pick cards to discard during a draw")},
    {"1-8", N_("Choose the bet or raise amount")},
    {"F1", N_("Show or hide this key list")},
    {"F11", N_("Toggle fullscreen")},
    {"Esc", N_("Close this list, or quit the game")},
};

const size_t g_fixed_key_count = sizeof(g_fixed_keys) / sizeof(g_fixed_keys[0]);
