/*
 button.h
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

#ifndef __BUTTON_H
#define __BUTTON_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "graphics.h"

#define CLICKED_DEFAULT                                                                            \
  {                                                                                                \
      .start_time = 0,                                                                             \
      .duration = 80,                                                                              \
  }

typedef struct {
  uint32_t start_time;
  uint32_t duration; // in milliseconds
} Clicked_t;

typedef struct {
  SDL_Color bg;
  SDL_Color fg;
} Color_t;

typedef struct {
  EColorName_t bg;
  EColorName_t fg;
} EColor_t;

typedef struct {
  const char *text;
  SDL_Renderer *renderer;
  Color_t color;
  SDL_Rect rect;
  TTF_Font *font;
  bool hovered, enabled, selected;
  bool active;
  SDL_Keycode hotkey;
  Clicked_t click;
} Button_t;

void render_button(Button_t *button);

Button_t create_button(const char *text, EColor_t color, TTF_Font *font, const SDL_Keycode key);

#endif
