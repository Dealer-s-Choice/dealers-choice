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

#include <SDL2/SDL_image.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#define ATLAS_FACES 15 /* indices 1..13 used; slot 0 wasted */
#define ATLAS_COLORS 2 /* 0 = black (spades/clubs), 1 = red (hearts/diamonds) */
#define ATLAS_SUITS 4

static struct {
  SDL_Texture *texture;
  int w, h;
} g_face_atlas[ATLAS_FACES][ATLAS_COLORS];

static struct {
  SDL_Texture *texture;
  int w, h;
} g_suit_atlas[ATLAS_SUITS];

static bool g_initialised = false;
static SDL_Renderer *g_renderer = NULL;
static TTF_Font *g_face_font = NULL;
static char g_data_dir[4096] = {0};

static int color_idx_for_suit(int suit) {
  return (suit == DH_SUIT_HEARTS || suit == DH_SUIT_DIAMONDS) ? 1 : 0;
}

/* Maps DH_SUIT_* to the corresponding SVG filename under
 * data/images/suits/. */
static const char *suit_svg_name(int suit) {
  switch (suit) {
  case DH_SUIT_HEARTS:
    return "heart.svg";
  case DH_SUIT_DIAMONDS:
    return "diamond.svg";
  case DH_SUIT_SPADES:
    return "spade.svg";
  case DH_SUIT_CLUBS:
    return "club.svg";
  default:
    return NULL;
  }
}

static void destroy_locked(void) {
  for (int f = 0; f < ATLAS_FACES; f++)
    for (int c = 0; c < ATLAS_COLORS; c++) {
      if (g_face_atlas[f][c].texture) {
        SDL_DestroyTexture(g_face_atlas[f][c].texture);
        g_face_atlas[f][c].texture = NULL;
      }
      g_face_atlas[f][c].w = g_face_atlas[f][c].h = 0;
    }
  for (int s = 0; s < ATLAS_SUITS; s++) {
    if (g_suit_atlas[s].texture) {
      SDL_DestroyTexture(g_suit_atlas[s].texture);
      g_suit_atlas[s].texture = NULL;
    }
    g_suit_atlas[s].w = g_suit_atlas[s].h = 0;
  }
}

void card_text_atlas_init(SDL_Renderer *renderer, TTF_Font *face_font, const char *data_dir) {
  if (g_initialised && g_renderer == renderer && g_face_font == face_font &&
      strcmp(g_data_dir, data_dir) == 0)
    return;
  destroy_locked();
  g_renderer = renderer;
  g_face_font = face_font;
  snprintf(g_data_dir, sizeof(g_data_dir), "%s", data_dir);
  g_initialised = false;

  /* Same color convention as make_human_readable_card in src/client.c:
   * red for hearts and diamonds, black for spades and clubs. */
  const SDL_Color colors[ATLAS_COLORS] = {
      [0] = {0, 0, 0, 255},
      [1] = {200, 0, 0, 255},
  };

  for (int face = DH_CARD_ACE; face <= DH_CARD_KING; face++) {
    const char *face_str = DH_get_card_face_str(face);
    if (!face_str)
      continue;
    for (int c = 0; c < ATLAS_COLORS; c++) {
      SDL_Surface *surface = TTF_RenderUTF8_Blended(face_font, face_str, colors[c]);
      if (!surface) {
        fprintf(stderr, "card_text_atlas_init: face render (%s) failed: %s\n", face_str,
                TTF_GetError());
        continue;
      }
      SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
      if (!texture) {
        fprintf(stderr, "card_text_atlas_init: face texture (%s) failed: %s\n", face_str,
                SDL_GetError());
        SDL_FreeSurface(surface);
        continue;
      }
      g_face_atlas[face][c].texture = texture;
      g_face_atlas[face][c].w = surface->w;
      g_face_atlas[face][c].h = surface->h;
      SDL_FreeSurface(surface);
    }
  }

  /* Load each suit SVG white-on-transparent.  The SVG files bake in
   * width/height attributes (currently 96×96) — chosen oversize so
   * SDL downscales them cleanly to whatever display size card.c
   * picks, and so they stay crisp in fullscreen / high-DPI modes
   * where SDL_RenderSetLogicalSize would otherwise upscale a
   * smaller-rasterised texture.  card_widget_render applies the
   * per-suit colour via SDL_SetTextureColorMod, so one texture
   * serves both red and black variants. */
  for (int suit = 0; suit < ATLAS_SUITS; suit++) {
    const char *svg_name = suit_svg_name(suit);
    if (!svg_name)
      continue;
    char path[4096];
    snprintf(path, sizeof(path), "%s/images/suits/%s", data_dir, svg_name);
    SDL_RWops *rw = SDL_RWFromFile(path, "rb");
    if (!rw) {
      /* SDL_RWFromFile sets both SDL_GetError() and errno on failure.
       * errno via strerror gives the human-friendly OS reason
       * ("No such file or directory", "Permission denied"); SDL's
       * own string is usually less specific. */
      fprintf(stderr, "card_text_atlas_init: open %s failed: %s (SDL: %s)\n", path, strerror(errno),
              SDL_GetError());
      continue;
    }
    SDL_Surface *surface = IMG_LoadSVG_RW(rw);
    SDL_RWclose(rw);
    if (!surface) {
      fprintf(stderr, "card_text_atlas_init: IMG_LoadSVG_RW(%s) failed: %s\n", path,
              IMG_GetError());
      continue;
    }
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) {
      fprintf(stderr, "card_text_atlas_init: suit texture (%s) failed: %s\n", svg_name,
              SDL_GetError());
      SDL_FreeSurface(surface);
      continue;
    }
    /* Enable color mod + alpha blending so card_widget_render can
     * tint the white SVG to the suit colour each frame. */
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    g_suit_atlas[suit].texture = texture;
    g_suit_atlas[suit].w = surface->w;
    g_suit_atlas[suit].h = surface->h;
    SDL_FreeSurface(surface);
  }

  g_initialised = true;
}

void card_text_atlas_destroy(void) {
  destroy_locked();
  g_renderer = NULL;
  g_face_font = NULL;
  g_data_dir[0] = '\0';
  g_initialised = false;
}

bool card_text_atlas_get(int face_val, int suit_val, CardAtlasEntry_t *out) {
  if (!g_initialised || !out)
    return false;
  if (face_val < DH_CARD_ACE || face_val > DH_CARD_KING)
    return false;
  if (suit_val < 0 || suit_val >= ATLAS_SUITS)
    return false;
  int c = color_idx_for_suit(suit_val);
  SDL_Texture *face = g_face_atlas[face_val][c].texture;
  SDL_Texture *suit = g_suit_atlas[suit_val].texture;
  if (!face || !suit)
    return false;
  out->face = face;
  out->face_w = g_face_atlas[face_val][c].w;
  out->face_h = g_face_atlas[face_val][c].h;
  out->suit = suit;
  out->suit_w = g_suit_atlas[suit_val].w;
  out->suit_h = g_suit_atlas[suit_val].h;
  return true;
}

bool card_text_atlas_is_initialised(void) { return g_initialised; }
