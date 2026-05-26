/*
 widgets/card_text_atlas.c
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

#include "card_text_atlas.h"

#include "deckhandler.h"
#include "style.h"

#include <stdio.h>

/* Atlas storage: indexed by [face_val][suit].  face_val runs 1..13
 * (DH_CARD_ACE..DH_CARD_KING) so we waste slot [0]; that's 13 * 4 *
 * sizeof(SDL_Texture*) = 416 bytes of wasted CPU pointers, negligible
 * vs the alternative of subtracting 1 every lookup and making the
 * code harder to read. */
#define ATLAS_FACES 15 /* 0..14 inclusive, indices 1..13 used */
#define ATLAS_SUITS 4

static struct {
  SDL_Texture *texture;
  int w, h;
} g_atlas[ATLAS_FACES][ATLAS_SUITS];

static bool g_initialised = false;
static SDL_Renderer *g_renderer = NULL;
static TTF_Font *g_font = NULL;

static void destroy_locked(void) {
  for (int f = 0; f < ATLAS_FACES; f++)
    for (int s = 0; s < ATLAS_SUITS; s++) {
      if (g_atlas[f][s].texture) {
        SDL_DestroyTexture(g_atlas[f][s].texture);
        g_atlas[f][s].texture = NULL;
      }
      g_atlas[f][s].w = g_atlas[f][s].h = 0;
    }
}

void card_text_atlas_init(SDL_Renderer *renderer, TTF_Font *font) {
  /* If already initialised for the same font, no-op so callers can
   * be defensive without burning cycles. */
  if (g_initialised && g_renderer == renderer && g_font == font)
    return;
  destroy_locked();
  g_renderer = renderer;
  g_font = font;
  g_initialised = false;

  for (int face = DH_CARD_ACE; face <= DH_CARD_KING; face++) {
    const char *face_str = DH_get_card_face_str(face);
    if (!face_str)
      continue;
    for (int suit = 0; suit < ATLAS_SUITS; suit++) {
      DH_Card card = {.face_val = face, .suit = suit};
      const char *suit_str = DH_get_card_unicode_suit(card);
      if (!suit_str)
        continue;
      char text[24];
      snprintf(text, sizeof(text), "%s%s", face_str, suit_str);

      /* Same color convention as make_human_readable_card in
       * src/client.c: red for hearts and diamonds, black for spades
       * and clubs.  Kept inline rather than via get_color() to avoid
       * an interface cycle with the styles config. */
      SDL_Color color = (suit == DH_SUIT_HEARTS || suit == DH_SUIT_DIAMONDS)
                            ? (SDL_Color){200, 0, 0, 255}
                            : (SDL_Color){0, 0, 0, 255};

      SDL_Surface *surface = TTF_RenderUTF8_Blended(font, text, color);
      if (!surface) {
        fprintf(stderr, "card_text_atlas_init: TTF_RenderUTF8_Blended(%s) failed: %s\n",
                text, TTF_GetError());
        continue;
      }
      SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
      if (!texture) {
        fprintf(stderr, "card_text_atlas_init: SDL_CreateTextureFromSurface(%s) failed: %s\n",
                text, SDL_GetError());
        SDL_FreeSurface(surface);
        continue;
      }
      g_atlas[face][suit].texture = texture;
      g_atlas[face][suit].w = surface->w;
      g_atlas[face][suit].h = surface->h;
      SDL_FreeSurface(surface);
    }
  }
  g_initialised = true;
}

void card_text_atlas_destroy(void) {
  destroy_locked();
  g_renderer = NULL;
  g_font = NULL;
  g_initialised = false;
}

SDL_Texture *card_text_atlas_get(int face_val, int suit_val, int *out_w, int *out_h) {
  if (!g_initialised)
    return NULL;
  if (face_val < DH_CARD_ACE || face_val > DH_CARD_KING)
    return NULL;
  if (suit_val < 0 || suit_val >= ATLAS_SUITS)
    return NULL;
  SDL_Texture *t = g_atlas[face_val][suit_val].texture;
  if (!t)
    return NULL;
  if (out_w)
    *out_w = g_atlas[face_val][suit_val].w;
  if (out_h)
    *out_h = g_atlas[face_val][suit_val].h;
  return t;
}

bool card_text_atlas_is_initialised(void) { return g_initialised; }
