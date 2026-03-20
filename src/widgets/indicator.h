/*
 indicator.h
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

#ifndef __IND_H
#define __IND_H

#include "graphics.h"
#include "text.h"
#include "ui_widget.h"

typedef struct {
  UIWidget_t base;

  TextWidget_t *text; // must be second member — see TextWrapperWidget_t

  SDL_Renderer *renderer;

  SDL_Color bg_color;

  int cx, cy; // oval center
  int rx, ry; // oval radii
} Indicator_t;
/*
ChatGPT:
The reason `fg_color` (foreground/text color) is not stored in the struct is
because in this design, the **text is rendered immediately to a texture**
during `create_indicator()`.

Once the texture is created with `SDL_CreateTextureFromSurface()`, the
color information is baked into the texture**. * You no longer need the
`fg_color` stored in the struct, because rendering the indicator simply draws
the pre-created texture.

*/

Indicator_t *create_indicator(const char *text, TTF_Font *font, EColorName_t bg_color,
                              EColorName_t fg_color);

#endif
