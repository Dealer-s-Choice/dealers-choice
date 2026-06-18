/*
 menus.h
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

/* The pre-game menu screens (connect / settings), driven by main(). */

#ifndef __MENUS_H
#define __MENUS_H

#include "client.h"
#include "util.h"

/* Return codes from the connect screen telling main() what to do next. */
enum {
  RUN_CLIENT = 20,
  RUN_SETTINGS = 21,
};

int menu_display_connect(PlayerConfig_t *player_config, char *host_str, uint16_t *port,
                         SdlContext_t *sdl_context, Font_t *font, LinkWidget_t **links);

void menu_display_settings(PlayerConfig_t *player_config, SdlContext_t *sdl_context, Font_t *font,
                           const Path_t *path);

#endif
