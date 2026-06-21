/*
 widgets/card.h
 https://github.com/Dealer-s-Choice/dealers_choice

 MIT License

 Copyright (c) 2025,2026 Andy Alt

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

#ifndef __CARD_WIDGET_H
#define __CARD_WIDGET_H

#include "ui_widget.h"
#include <SDL2/SDL_ttf.h>

#define SIZEOF_CARD_TEXT 20

typedef struct {
  UIWidget_t base; /* base.hovered and base.selected are used for interaction */
  /* text[] is kept for debug / external observers; rendering goes through
   * card_text_atlas_get(face_val, suit) so the string itself is no longer
   * on the render hot path. */
  char text[SIZEOF_CARD_TEXT];
  /* Card identity used by the atlas lookup in card_widget_render.
   * Set by make_human_readable_card() when the card slot is populated;
   * unused when is_back or is_null. */
  int face_val;
  int suit;
  bool is_back;
  bool is_null;
  bool is_wild;
  bool is_winning;
  bool is_shaded; /* local player's own card that is hidden from other players
                   * (stud/holdem hole cards); drawn with the eye-off badge (#64) */
  bool my_card;   /* true when this card belongs to the local player */
  TTF_Font *font; /* kept for the card-back animated patterns that still
                   * compute glyph metrics; not used for face rendering. */
} CardWidget_t;

void card_widget_init(CardWidget_t *cw, TTF_Font *font);
void card_widget_select_back_for_game(void);

#endif
