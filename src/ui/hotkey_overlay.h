/*
 hotkey_overlay.h
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
 * Shared F1 "Keys" reference overlay, used by every screen that has its own
 * event loop: the connect screen (menus.c), the lobby (game_select.c), and the
 * gameplay screen (game_logic.c).  It reads the single-source hotkey tables in
 * hotkey_table.h, so the on-screen list, the docs (keys.md), and the running
 * game can never disagree.
 *
 * The overlay is NON-BLOCKING by design: hotkey_overlay_render draws one frame
 * of a read-only panel and returns immediately, so the caller's loop keeps
 * polling the socket (the gameplay screen's action timer is never starved).
 * The caller owns the visibility flag and toggles it via
 * hotkey_overlay_handle_event.
 */

#ifndef __HOTKEY_OVERLAY_H
#define __HOTKEY_OVERLAY_H

#include <SDL2/SDL.h>
#include <stdbool.h>

#include "graphics.h" /* Font_t */

/* Draw the overlay over the current frame.  Call last, just before
 * SDL_RenderPresent, while the caller's visibility flag is true.
 *
 * in_game selects the content: true shows the configurable action keys (live
 * bindings from g_hotkey_cfg) plus every fixed key; false (connect screen /
 * lobby) shows only the keys that work outside a hand, since the poker action
 * keys and the 1-5 / 1-8 selectors have no meaning there. */
void hotkey_overlay_render(SDL_Renderer *renderer, const Font_t *font, bool in_game);

/* Feed each polled event through this before the caller's own handling.
 * Returns true when the event was consumed (the caller should skip it):
 *   - F1 toggles *visible.
 *   - While *visible, Esc closes the panel and every other key is swallowed,
 *     so a key pressed while reading the list does not also fire its action.
 * SDL_QUIT and all non-keydown events fall through (return false). */
bool hotkey_overlay_handle_event(const SDL_Event *e, bool *visible);

#endif
