/*
 main.c
 https://github.com/Dealer-s-Choice/dealers_choice

 MIT License

 Copyright (c) 2025 Andy Alt

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

#include <stdio.h>
#include <string.h>

#include "client.h"
#include "graphics.h"
#include "main.h"
#include "server.h"

lua_State *init_lua() {
  lua_State *L = luaL_newstate();
  luaL_openlibs(L);
  if (luaL_dofile(L, "../src/main.lua") != LUA_OK) {
    fprintf(stderr, "Lua error: %s\n", lua_tostring(L, -1));
    return NULL;
  }
  return L;
}

const char *send_input(lua_State *L, const char *key) {
  lua_getglobal(L, "Menu");
  lua_getfield(L, -1, "handle_input");
  lua_pushvalue(L, -2); // self
  lua_pushstring(L, key);
  if (lua_pcall(L, 2, 1, 0) != LUA_OK) {
    fprintf(stderr, "Lua error: %s\n", lua_tostring(L, -1));
    return NULL;
  }

  const char *result = NULL;
  if (!lua_isnil(L, -1))
    result = lua_tostring(L, -1);
  lua_pop(L, 2); // result + Menu
  return result;
}

int render_menu(SDL_Renderer *renderer, TTF_Font *font, lua_State *L) {
  lua_getglobal(L, "Menu");
  lua_getfield(L, -1, "get_labels");
  lua_pushvalue(L, -2); // self
  if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
    fprintf(stderr, "Lua error: %s\n", lua_tostring(L, -1));
    return -1;
  }

  SDL_Color color = {255, 255, 255, 255};
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  SDL_RenderClear(renderer);

  if (lua_istable(L, -1)) {
    int y = 100;
    lua_pushnil(L);
    while (lua_next(L, -2)) {
      const char *text = lua_tostring(L, -1);
      SDL_Surface *surf = TTF_RenderUTF8_Blended(font, text, color);
      SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
      SDL_Rect dst = {100, y, surf->w, surf->h};
      SDL_RenderCopy(renderer, tex, NULL, &dst);
      SDL_FreeSurface(surf);
      SDL_DestroyTexture(tex);
      lua_pop(L, 1);
      y += 40;
    }
  }

  SDL_RenderPresent(renderer);
  lua_pop(L, 2); // table + Menu
  return 0;
}

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  struct sdl_context_t sdl_context;
  init_sdl_window(&sdl_context, "Dealer's Choice");
  if (TTF_Init() == -1) {
    fprintf(stderr, "TTF_Init: %s\n", TTF_GetError());
    return -1;
  }

  lua_State *L = init_lua();
  if (!L)
    return 1;

  struct font_t font;

  font.fonts[OTHER] = open_font(&font_args[OTHER]);
  if (!font.fonts[OTHER])
    return -1;

  bool running = true;
  const char *action = NULL;
  while (running && !action) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT)
        running = false;
      if (e.type == SDL_KEYDOWN) {
        switch (e.key.keysym.sym) {
        case SDLK_UP:
          action = send_input(L, "up");
          break;
        case SDLK_DOWN:
          action = send_input(L, "down");
          break;
        case SDLK_RETURN:
          action = send_input(L, "return");
          break;
        case SDLK_ESCAPE:
          running = false;
          break;
        }
      }
    }
    render_menu(sdl_context.renderer, font.fonts[OTHER], L);
    SDL_Delay(16);
  }

  if (action)
    printf("Action chosen: %s\n", action);

  lua_close(L);
  TTF_CloseFont(font.fonts[OTHER]);
  SDL_DestroyRenderer(sdl_context.renderer);
  SDL_DestroyWindow(sdl_context.window);
  TTF_Quit();
  SDL_Quit();

  return 0;
}
