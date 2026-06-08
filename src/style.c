/*
 style.c
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

#include "style.h"
#include "graphics.h"

// clang-format off
const ButtonRole_t ROLE_PRIMARY = { {  0,   0,   0, 255}, {255, 255,   0, 255}, FONT_BOLD, 20, 10 };
const ButtonRole_t ROLE_DANGER  = { {255, 255, 255, 255}, {255,   0,   0, 255}, FONT_BOLD, 20, 10 };
const ButtonRole_t ROLE_ALT    = { {255, 255, 255, 255}, {165,  42,  42, 255}, FONT_BOLD, 20, 10 };
const ButtonRole_t ROLE_CANCEL  = { {255, 255, 255, 255}, {128, 128, 128, 255}, FONT_BOLD, 20, 10 };

const SDL_Color DC_INDICATOR_WILD_BG = {128,   0, 128, 255};
const SDL_Color DC_INDICATOR_WILD_FG = {255, 255, 255, 255};
const SDL_Color DC_INDICATOR_GAME_BG = {255, 165,   0, 255};
const SDL_Color DC_INDICATOR_GAME_FG = {  0,   0,   0, 255};

const SDL_Color DC_TEXT_ON_DARK  = {255, 255, 255, 255};
const SDL_Color DC_DISCARD_TEXT  = {255, 255,   0, 255};
const SDL_Color DC_DASH_BG       = {  0,   0,   0, 130};
const SDL_Color DC_DASH_DIVIDER  = { 25,  75,  35, 255};
const SDL_Color DC_TEXT_ON_LIGHT = {  0,   0,   0, 255};
const SDL_Color DC_TEXT_MUTED    = {200, 200, 200, 255};
const SDL_Color DC_LINK_NORMAL   = {  0,   0,   0, 255};
const SDL_Color DC_LINK_HOVER    = {  0,   0, 255, 255};
const SDL_Color DC_TIMER_BG      = {  0, 125,   0, 255};
const SDL_Color DC_TIMER_ELAPSED = {240, 204,  48, 255};
// clang-format on
