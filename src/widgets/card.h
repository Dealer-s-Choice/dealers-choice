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

#define CARD_W 80
#define CARD_H 50
#define CARD_PADDING 10
#define SIZEOF_CARD_TEXT 20

typedef struct {
  UIWidget_t base; /* base.hovered and base.selected are used for interaction */
  char text[SIZEOF_CARD_TEXT];
  SDL_Color textColor;
  bool is_back;
  bool is_null;
  bool is_wild;
  bool is_winning;
  bool my_card; /* true when this card belongs to the local player */
  TTF_Font *font;
} CardWidget_t;

void card_widget_init(CardWidget_t *cw, TTF_Font *font);
void card_widget_select_back_for_game(void);

#endif
