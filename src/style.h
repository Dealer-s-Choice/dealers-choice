/*
 style.h
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

#ifndef __STYLE_H
#define __STYLE_H

#include <SDL2/SDL.h>

typedef struct {
  struct { SDL_Color bg, fg; } button_primary;   /* main actions: black bg, yellow fg */
  struct { SDL_Color bg, fg; } button_danger;    /* quit/X: white bg, red fg */
  struct { SDL_Color bg, fg; } button_warn;      /* kick/ban/bet amounts: brown bg, white fg */
  struct { SDL_Color bg, fg; } button_cancel;    /* cancel dialogs: white bg, gray fg */
  SDL_Color text_on_dark;   /* text on dark/colored backgrounds */
  SDL_Color text_on_light;  /* text on light backgrounds */
  SDL_Color text_muted;     /* subdued labels */
  SDL_Color link_normal;
  SDL_Color link_hover;
  struct { SDL_Color bg, fg; } indicator_wild;
  struct { SDL_Color bg, fg; } indicator_game;
  SDL_Color timer_bg;       /* inner circle fill */
  SDL_Color timer_elapsed;  /* elapsed-time wedge */
  int button_font;  /* FONT_* index */
  int link_font;    /* FONT_* index */
} StyleConfig_t;

extern StyleConfig_t g_style_cfg;
StyleConfig_t get_style_config(const char *data_dir);

#endif
