/*
 widgets/card_text_atlas.h
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

#ifndef __CARD_TEXT_ATLAS_H
#define __CARD_TEXT_ATLAS_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>

/* Builds a per-(face, suit) text texture for every card in the deck and
 * caches them for the lifetime of the GUI session.  Replaces the
 * per-frame TTF_RenderUTF8_Blended + SDL_CreateTextureFromSurface
 * pair that card_widget_render used to do — now that path is a single
 * lookup + SDL_RenderCopy per card per frame, zero allocations.
 *
 * Must be called after SDL + TTF are initialised and after the
 * renderer + font are created.  Safe to call multiple times
 * (subsequent calls reuse the existing atlas if the font hasn't
 * changed; pass a different font to rebuild).
 *
 * `font` is the TTF_Font used for card faces — typically
 * font->fonts[FONT_CARD] in the client.  The atlas does NOT take
 * ownership of the font or renderer; callers retain responsibility
 * for those.
 */
void card_text_atlas_init(SDL_Renderer *renderer, TTF_Font *font);

/* Frees every cached texture and resets the atlas.  Call before
 * destroying the SDL_Renderer the atlas was initialised with —
 * SDL_DestroyTexture on a texture whose renderer is already gone is
 * undefined behavior. */
void card_text_atlas_destroy(void);

/* Returns the cached texture for the given face/suit pair, plus its
 * pixel dimensions.  Returns NULL (with *out_w / *out_h untouched)
 * when:
 *   - the atlas hasn't been initialised yet,
 *   - the (face_val, suit) is outside the dealt range
 *     (face_val 1..13, suit 0..3),
 *   - or the initial render for this card failed and the slot is
 *     empty.  Callers should fall back to a draw-time render in that
 *     case (or, more typically, treat it as a fatal init bug).
 *
 * The returned texture is owned by the atlas — do not call
 * SDL_DestroyTexture on it. */
SDL_Texture *card_text_atlas_get(int face_val, int suit_val, int *out_w, int *out_h);

/* True once card_text_atlas_init has succeeded.  Tests and asserts
 * use this to confirm the lifecycle without leaking the internal
 * static arrays. */
bool card_text_atlas_is_initialised(void);

/* Total number of textures the atlas holds when fully populated.
 * 13 faces × 4 suits = 52.  Exposed for tests so they don't have to
 * encode the constant separately. */
#define CARD_TEXT_ATLAS_SIZE 52

#endif
