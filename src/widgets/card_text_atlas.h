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

/* Returned by card_text_atlas_get: the two pre-rendered textures and
 * their dimensions.  card_widget_render lays them out side-by-side
 * (face on the left, suit on the right, pair centered in the card
 * rect).
 *
 * Suit textures are loaded white-on-transparent from SVG and tinted
 * to red or black per suit via SDL_SetTextureColorMod at render time.
 * Face textures are TTF-rendered and already coloured per suit. */
typedef struct {
  SDL_Texture *face;
  SDL_Texture *suit;
  int face_w, face_h;
  int suit_w, suit_h;
} CardAtlasEntry_t;

/* Builds two atlases:
 *
 *   - face_atlas[13][2]  — face glyph ("A","2",...,"K") in black and
 *                          red, TTF-rendered with `face_font`.  Index
 *                          is (face_val, color_idx) where color_idx
 *                          = 1 for hearts/diamonds, 0 for spades/clubs.
 *
 *   - suit_atlas[4]      — suit symbol (♠/♥/♦/♣) loaded from
 *                          `data_dir`/images/suits/{spade,heart,
 *                          diamond,club}.svg via IMG_LoadSizedSVG_RW.
 *                          Rendered white-on-transparent; the colour
 *                          is applied per-render via
 *                          SDL_SetTextureColorMod.
 *
 * 26 + 4 = 30 textures total.
 *
 * Must be called after SDL + TTF + SDL_image + the renderer are up
 * and after the face font is open.  Safe to call multiple times —
 * subsequent calls reuse the existing atlas if the inputs are
 * unchanged.  The atlas does NOT take ownership of the font or
 * renderer. */
void card_text_atlas_init(SDL_Renderer *renderer, TTF_Font *face_font, const char *data_dir);

/* Frees every cached texture and resets the atlas.  Call before
 * destroying the SDL_Renderer the atlas was initialised with —
 * SDL_DestroyTexture on a texture whose renderer is already gone is
 * undefined behavior. */
void card_text_atlas_destroy(void);

/* Populates *out with the face + suit textures for the given card.
 * Returns false (with *out untouched) when:
 *   - the atlas hasn't been initialised yet,
 *   - the (face_val, suit) is outside the dealt range
 *     (face_val 1..13, suit 0..3),
 *   - or either of the initial loads for this card failed.
 *
 * The returned textures are owned by the atlas — do not call
 * SDL_DestroyTexture on them. */
bool card_text_atlas_get(int face_val, int suit_val, CardAtlasEntry_t *out);

/* True once card_text_atlas_init has succeeded. */
bool card_text_atlas_is_initialised(void);

/* Total textures the atlas holds when fully populated: 13 faces × 2
 * colors + 4 suits = 30. */
#define CARD_TEXT_ATLAS_SIZE 30

#endif
