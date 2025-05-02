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

enum { RUN_CLIENT = 20 };

static int menu_display_connect(char *input_text) {
  struct sdl_context_t sdl_context;
  init_sdl_window(&sdl_context, "Dealer's Choice", 500, 500);

  struct font_t font;

  font.fonts[OTHER] = open_font(&font_args[OTHER]);
  if (!font.fonts[OTHER])
    return -1;

  SDL_Rect connect_button = make_rect(100, 160, 120, 40);
  struct button_t button_connect = {
      .text = "Connect",
      .renderer = sdl_context.renderer,
      .bg_color = get_color(COLOR_BLACK),
      .fg_color = get_color(COLOR_YELLOW),
      .rect = connect_button,
      .pos = {100, 160},
      .font = font.fonts[OTHER],
  };

  SDL_Rect input_box = make_rect(100, 220, 200, 40);
  SDL_StartTextInput();

  bool run_client = false;
  bool running = true;
  while (running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      int mx = e.button.x;
      int my = e.button.y;
      button_connect.hovered = SDL_PointInRect(&(SDL_Point){mx, my}, &connect_button);
      if (e.type == SDL_QUIT) {
        running = false;
      } else if (e.type == SDL_MOUSEBUTTONDOWN) {
        if (point_in_rect(mx, my, &connect_button)) {
          run_client = true;
          running = false;
        }
      } else if (e.type == SDL_TEXTINPUT) {
        if (strlen(input_text) + strlen(e.text.text) < MAX_INPUT_LENGTH) {
          strcat(input_text, e.text.text);
        }
      } else if (e.type == SDL_KEYDOWN) {
        if (e.key.keysym.sym == SDLK_BACKSPACE && strlen(input_text) > 0) {
          input_text[strlen(input_text) - 1] = '\0';
        } else if (e.key.keysym.sym == SDLK_RETURN) {
          run_client = true;
          running = false;
        }
      }
    }

    // Clear screen
    SDL_SetRenderDrawColor(sdl_context.renderer, get_color(COLOR_GREEN_ONE).r,
                           get_color(COLOR_GREEN_ONE).g, get_color(COLOR_GREEN_ONE).b,
                           get_color(COLOR_GREEN_ONE).a);
    SDL_RenderClear(sdl_context.renderer);

    make_button(&button_connect);

    SDL_SetRenderDrawColor(sdl_context.renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(sdl_context.renderer, &input_box);
    SDL_Rect input_text_pos = {input_box.x, input_box.y, 0, 0};
    render_text(sdl_context.renderer, font.fonts[OTHER], input_text, get_color(COLOR_WHITE),
                &input_text_pos);

    SDL_RenderPresent(sdl_context.renderer);
    SDL_Delay(16);
  }

  SDL_StopTextInput();
  TTF_CloseFont(font.fonts[OTHER]);
  do_sdl_cleanup(&sdl_context);

  return run_client == true ? RUN_CLIENT : 0;
}

int main(int argc, char *argv[]) {
  if (argc == 2) {
    if (strcmp("--server", argv[1]) == 0)
      return run_server();
    else
      printf("Usage:\n\n\
  %s --server",
             argv[0]);
  }

  if (SDL_Init(SDL_INIT_VIDEO) == -1 || SDLNet_Init() == -1) {
    fprintf(stderr, "SDL or SDL_net init failed: %s\n", SDLNet_GetError());
    return 1;
  }

  if (TTF_Init() == -1) {
    fprintf(stderr, "TTF_Init: %s\n", TTF_GetError());
    return -1;
  }

  char addr[MAX_INPUT_LENGTH] = "127.0.0.1";
  if (menu_display_connect(addr) == RUN_CLIENT) {
    printf("Attempting to connect to: %s\n", addr);
    run_client(addr);
  }

  TTF_Quit();
  SDL_Quit();
  return 0;
}
