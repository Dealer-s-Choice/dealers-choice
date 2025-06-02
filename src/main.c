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

static int menu_display_connect(char *input_text, SDL_Renderer *renderer, Font_t *font) {
  Button_t button_connect = {
      .text = "Connect",
      .renderer = renderer,
      .bg_color = get_color(COLOR_BLACK),
      .fg_color = get_color(COLOR_YELLOW),
      .rect = {100, 160, 120, 40},
      .font = font->fonts[OTHER],
      .enabled = true,
  };

  SDL_Rect input_box = make_rect(100, 220, 200, 40);
  SDL_StartTextInput();

  bool run_client = false;
  bool running = true;
  while (running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      SDL_Point mouse_pos = {e.button.x, e.button.y};
      button_connect.hovered = SDL_PointInRect(&mouse_pos, &button_connect.rect);
      if (e.type == SDL_QUIT) {
        running = false;
      } else if (e.type == SDL_MOUSEBUTTONDOWN) {
        if (SDL_PointInRect(&mouse_pos, &button_connect.rect)) {
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
    clear_screen(renderer);

    render_button(&button_connect);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &input_box);
    SDL_Rect input_text_pos = {input_box.x, input_box.y, 0, 0};
    render_text(renderer, font->fonts[OTHER], input_text, get_color(COLOR_WHITE), &input_text_pos);

    SDL_RenderPresent(renderer);
    SDL_Delay(16);
  }

  SDL_StopTextInput();
  return run_client == true ? RUN_CLIENT : 0;
}

int main(int argc, char *argv[]) {
  const char *bind_address = NULL; // Default is NULL, meaning "0.0.0.0"
  bool test_mode = false;
  bool run_server_flag = false;

  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--server") == 0) {
      run_server_flag = true;
    } else if (strcmp(argv[i], "---test") == 0) {
      test_mode = true;
    } else if (strcmp(argv[i], "--bind-address") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "Error: --bind-address requires an argument\n");
        exit(EXIT_FAILURE);
      }
      bind_address = argv[++i]; // Advance i to get the argument
    } else {
      // Unrecognized arg
      fprintf(stderr,
              "Usage:\n"
              "  %s\n"
              "  %s --server [--bind-address IP]\n",
              argv[0], argv[0]);
      exit(EXIT_FAILURE);
    }
  }

  if (run_server_flag) {
    return run_server(bind_address, test_mode);
  }

  if (SDL_Init(SDL_INIT_VIDEO) == -1 || SDLNet_Init() == -1) {
    fprintf(stderr, "SDL or SDL_net init failed: %s\n", SDLNet_GetError());
    return 1;
  }

  if (TTF_Init() == -1) {
    fprintf(stderr, "TTF_Init: %s\n", TTF_GetError());
    return -1;
  }

  SdlContext_t sdl_context;
  init_sdl_window(&sdl_context, "Dealer's Choice");

  Font_t font;
  for (int i = 0; i < NUM_FONTS; ++i) {
    font.fonts[i] = open_font(&font_args[i]);
    if (!font.fonts[i])
      return -1;
  }

  char addr[MAX_INPUT_LENGTH] = "127.0.0.1";
  if (menu_display_connect(addr, sdl_context.renderer, &font) == RUN_CLIENT) {
    printf("Attempting to connect to: %s\n", addr);
    get_socket_context_and_run_client(addr, &sdl_context, &font, test_mode);
  }

  for (int i = 0; i < NUM_FONTS; ++i)
    TTF_CloseFont(font.fonts[i]);
  TTF_Quit();
  do_sdl_cleanup(&sdl_context);

  return 0;
}
