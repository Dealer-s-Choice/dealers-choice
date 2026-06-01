/*
 hotkeys.c
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

#include "hotkeys.h"

#include <stdio.h>

HotkeyConfig_t g_hotkey_cfg;

static SDL_Keycode resolve(const char *name, const char *label) {
  if (!name || !*name)
    return SDLK_UNKNOWN;
  SDL_Keycode kc = SDL_GetKeyFromName(name);
  if (kc == SDLK_UNKNOWN)
    fprintf(stderr, "init_hotkeys: unrecognised key name '%s' for %s\n", name, label);
  return kc;
}

void init_hotkeys(const PlayerConfig_t *cfg) {
  g_hotkey_cfg.check    = resolve(cfg->hotkey_check,    "hotkey_check");
  g_hotkey_cfg.bet      = resolve(cfg->hotkey_bet,      "hotkey_bet");
  g_hotkey_cfg.fold     = resolve(cfg->hotkey_fold,     "hotkey_fold");
  g_hotkey_cfg.call     = resolve(cfg->hotkey_call,     "hotkey_call");
  g_hotkey_cfg.raise    = resolve(cfg->hotkey_raise,    "hotkey_raise");
  g_hotkey_cfg.complete = resolve(cfg->hotkey_complete, "hotkey_complete");
  g_hotkey_cfg.discard  = resolve(cfg->hotkey_discard,  "hotkey_discard");
}
