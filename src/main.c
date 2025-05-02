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


#define MAX_INPUT_LENGTH 64

SDL_Rect make_rect(int x, int y, int w, int h) {
  SDL_Rect r = {x, y, w, h};
  return r;
}

bool point_in_rect(int x, int y, SDL_Rect *r) {
  return x >= r->x && x <= (r->x + r->w) && y >= r->y && y <= (r->y + r->h);
}

void render_text(SDL_Renderer *renderer, TTF_Font *font, const char *text, SDL_Color color, SDL_Rect *dest) {
  if (!text || strlen(text) == 0) {
      fprintf(stderr, "Warning: Empty or null text passed to render_text.\n");
      return;
  }
  SDL_Surface *surface = TTF_RenderUTF8_Blended(font, text, color);
  if (!surface) {
      fprintf(stderr, "TTF_RenderUTF8_Blended error: %s\n", TTF_GetError());
      return;
  }

  SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
  if (!texture) {
      fprintf(stderr, "SDL_CreateTextureFromSurface error: %s\n", SDL_GetError());
      SDL_FreeSurface(surface);
      return;
  }

  dest->w = surface->w;
  dest->h = surface->h;

  SDL_RenderCopy(renderer, texture, NULL, dest);

  SDL_FreeSurface(surface);
  SDL_DestroyTexture(texture);
}

struct button_t {
  const char *text;
  SDL_Renderer *renderer;
  SDL_Color bg_color;
  SDL_Color fg_color;
  SDL_Rect rect;
  TTF_Font *font;
  struct pos_t pos;
};

void make_button(struct button_t *button) {
  SDL_SetRenderDrawColor(button->renderer, button->bg_color.r, button->bg_color.g, button->bg_color.b, button->bg_color.a);
  SDL_RenderFillRect(button->renderer, &button->rect);
  SDL_Rect connect_text_pos = {button->pos.x, button->pos.y, 0, 0};
  render_text(button->renderer, button->font, button->text, button->fg_color, &connect_text_pos);
}

int main(int argc, char *argv[]) {
  if (argc == 2) {
    if (strcmp("--server", argv[1]) == 0)
      return run_server();
    else
      printf(
"Usage:\n\n\
  %s --server", argv[0]);
  }

  if (SDL_Init(SDL_INIT_VIDEO) == -1 || SDLNet_Init() == -1) {
    fprintf(stderr, "SDL or SDL_net init failed: %s\n", SDLNet_GetError());
    return 1;
  }

  struct sdl_context_t sdl_context;
  init_sdl_window(&sdl_context, "Dealer's Choice", 500, 500);

  if (TTF_Init() == -1) {
    fprintf(stderr, "TTF_Init: %s\n", TTF_GetError());
    return -1;
  }

  struct font_t font;

  font.fonts[OTHER] = open_font(&font_args[OTHER]);
  if (!font.fonts[OTHER])
    return -1;

  SDL_Rect connect_button = make_rect(100, 160, 120, 40);
  SDL_Rect input_box = make_rect(100, 220, 200, 40);

  bool running = true;

  char input_text[MAX_INPUT_LENGTH] = "127.0.0.1";

  SDL_StartTextInput();

  while (running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT) {
        running = false;
      } else if (e.type == SDL_MOUSEBUTTONDOWN) {
        int mx = e.button.x;
        int my = e.button.y;

        if (point_in_rect(mx, my, &connect_button)) {
          printf("Connect selected!\n");
        }
      } else if (e.type == SDL_TEXTINPUT) {
        if (strlen(input_text) + strlen(e.text.text) < MAX_INPUT_LENGTH) {
          strcat(input_text, e.text.text);
        }
      } else if (e.type == SDL_KEYDOWN) {
        if (e.key.keysym.sym == SDLK_BACKSPACE && strlen(input_text) > 0) {
          input_text[strlen(input_text) - 1] = '\0';
        } else if (e.key.keysym.sym == SDLK_RETURN) {
          printf("Attempting to connect to: %s\n", input_text);
          SDL_StopTextInput();
          TTF_CloseFont(font.fonts[OTHER]);
          do_sdl_cleanup(&sdl_context);
          return run_client(input_text);
          break;
        }
      }
    }

    // Clear screen
    SDL_SetRenderDrawColor(sdl_context.renderer, 30, 30, 30, 255);
    SDL_RenderClear(sdl_context.renderer);

    SDL_Color white = {255, 255, 255, 255};
    SDL_Color gray = {200, 200, 200, 255};

    struct button_t button_connect = (struct button_t){
      .text = "Connect",
      .renderer = sdl_context.renderer,
      .bg_color = { 70, 70, 70, 255 },
      .fg_color = white,
      .rect = connect_button,
      .pos.x = 100,
      .pos.y = 160,
      .font = font.fonts[OTHER],
    };
    make_button(&button_connect);

    SDL_SetRenderDrawColor(sdl_context.renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(sdl_context.renderer, &input_box);
    SDL_Rect input_text_pos = {input_box.x, input_box.y, 0, 0};
    render_text(sdl_context.renderer, font.fonts[OTHER], input_text, gray, &input_text_pos);

    SDL_RenderPresent(sdl_context.renderer);
    SDL_Delay(16);
  }

  TTF_CloseFont(font.fonts[OTHER]);
  TTF_Quit();
  SDL_StopTextInput();
  do_sdl_cleanup(&sdl_context);
  SDL_Quit();
  return 0;
}
