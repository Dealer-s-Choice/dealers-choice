/*
 player_widget.h
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

#ifndef __PLAYER_H
#define __PLAYER_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "graphics.h"
#include "ui_widget.h"

typedef struct {
  UIWidget_t base;

  SDL_Renderer *renderer;

  SDL_Texture *nick_tex;
  SDL_Texture *ping_tex;

  SDL_Rect nick_rect;
  SDL_Rect ping_rect;

  SDL_Color color;

  TTF_Font *font;

  int ping_column_x;
  int ping; // cached ping value
} PlayerWidget_t;

void player_widget_render(PlayerWidget_t *pw);

PlayerWidget_t *player_widget_create(const char *nick, bool dealer, uint32_t ping, TTF_Font *font);

void player_widget_update_ping(PlayerWidget_t *pw, int ping);

#endif
