/*
 graphics.c
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

#include "graphics.h"
#include "game.h"

static const SDL_Color color_table[COLOR_COUNT] = {
    [COLOR_WHITE] = {255, 255, 255, 255}, [COLOR_LIGHTGRAY] = {200, 200, 200, 255},
    [COLOR_GRAY] = {128, 128, 128, 255},  [COLOR_DARKGRAY] = {64, 64, 64, 255},
    [COLOR_BLACK] = {0, 0, 0, 255},       [COLOR_RED] = {255, 0, 0, 255},
    [COLOR_GREEN] = {0, 255, 0, 255},     [COLOR_GREEN_ONE] = {0, 125, 0, 255},
    [COLOR_BLUE] = {0, 0, 255, 255},      [COLOR_YELLOW] = {255, 255, 0, 255},
    [COLOR_CYAN] = {0, 255, 255, 255},    [COLOR_MAGENTA] = {255, 0, 255, 255},
    [COLOR_ORANGE] = {255, 165, 0, 255},  [COLOR_PURPLE] = {128, 0, 128, 255},
    [COLOR_BROWN] = {165, 42, 42, 255},   [COLOR_PINK] = {255, 192, 203, 255},
    [COLOR_TEAL] = {0, 128, 128, 255},
};

static const char *color_names[COLOR_COUNT] = {
    "white",  "lightgray", "gray",    "darkgray", "black",  "red",   "green", "green_one", "blue",
    "yellow", "cyan",      "magenta", "orange",   "purple", "brown", "pink",  "teal"};

SDL_Color get_color(EColorName_t name) {
  if (name < 0 || name >= COLOR_COUNT)
    return (SDL_Color){0, 0, 0, 255}; // fallback
  return color_table[name];
}

const char *get_color_name(EColorName_t name) {
  if (name < 0 || name >= COLOR_COUNT)
    return "unknown";
  return color_names[name];
}

const FontArgs_t font_args[] = {
    [CARD] = {.file = "../src/LiberationMono-Regular.ttf", .ptsize = 38},
    [OTHER] = {.file = "../src/LiberationSerif-Bold.ttf", .ptsize = 30},
    [STATUS_MSG] = {.file = "../src/LiberationSerif-Bold.ttf", .ptsize = 24},
};

void clear_screen(SDL_Renderer *renderer) {
  SDL_SetRenderDrawColor(renderer, get_color(COLOR_GREEN_ONE).r, get_color(COLOR_GREEN_ONE).g,
                         get_color(COLOR_GREEN_ONE).b, get_color(COLOR_GREEN_ONE).a);
  SDL_RenderClear(renderer);
}

void init_sdl_window(ESdlContext_t *sdl_context, const char *title) {
  SDL_Rect bounds;
  if (SDL_GetDisplayBounds(0, &bounds) == 0) {
    printf("Display 0 bounds: x=%d, y=%d, w=%d, h=%d\n", bounds.x, bounds.y, bounds.w, bounds.h);
  } else {
    puts(SDL_GetError());
    exit(EXIT_FAILURE);
  }

  float factor = 0.8;
  float w = bounds.w * factor;
  float h = bounds.h * factor;

  sdl_context->window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w,
                                         h, SDL_WINDOW_SHOWN);
  if (!sdl_context->window)
    puts(SDL_GetError());
  sdl_context->renderer = SDL_CreateRenderer(sdl_context->window, -1, SDL_RENDERER_ACCELERATED);
  if (!sdl_context->renderer)
    puts(SDL_GetError());

  int x, y;
  SDL_GetWindowSize(sdl_context->window, &x, &y);
  sdl_context->win_center.x = x / 2;
  sdl_context->win_center.y = y / 2;

  sdl_context->window_width = w;
  sdl_context->window_height = h;

  // SDL_RenderSetLogicalSize(sdl_context->renderer, 1920 * factor, 1080 * factor);

  return;
}

TTF_Font *open_font(const FontArgs_t *args) {
  TTF_Font *font = TTF_OpenFont(args->file, args->ptsize);
  if (!font)
    fprintf(stderr, "Failed to load font (%s): %s\n", args->file, TTF_GetError());
  return font;
}

void render_text_centered(SDL_Renderer *renderer, TTF_Font *font, const char *text, SDL_Color color,
                          SDL_Point center) {
  SDL_Surface *surface = TTF_RenderUTF8_Blended(font, text, color);
  if (!surface) {
    fprintf(stderr, "TTF_RenderUTF8_Blended failed: %s\n", TTF_GetError());
    return;
  }

  SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
  if (!texture) {
    fprintf(stderr, "SDL_CreateTextureFromSurface failed: %s\n", SDL_GetError());
    SDL_FreeSurface(surface);
    return;
  }

  int w, h;
  SDL_QueryTexture(texture, NULL, NULL, &w, &h);

  SDL_Rect dst = {.x = center.x - w / 2, .y = center.y - h / 2, .w = w, .h = h};

  SDL_RenderCopy(renderer, texture, NULL, &dst);

  SDL_DestroyTexture(texture);
  SDL_FreeSurface(surface);
}

SDL_Rect make_rect(int x, int y, int w, int h) {
  SDL_Rect r = {x, y, w, h};
  return r;
}

void render_text(SDL_Renderer *renderer, TTF_Font *font, const char *text, SDL_Color color,
                 SDL_Rect *dest) {
  if (!text)
    text = "";

  SDL_Surface *surface = TTF_RenderUTF8_Blended(font, *text ? text : " ", color);
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

  int text_width;
  TTF_SizeUTF8(font, text, &text_width, NULL);

  // Blink cursor every ~500ms
  if ((SDL_GetTicks() / 500) % 2 == 0) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 255);
    int cursor_x = dest->x + text_width;
    int cursor_y = dest->y;
    int cursor_h = dest->h;
    SDL_RenderDrawLine(renderer, cursor_x, cursor_y, cursor_x, cursor_y + cursor_h);
  }

  SDL_FreeSurface(surface);
  SDL_DestroyTexture(texture);
}

void render_text_plain(SDL_Renderer *renderer, TTF_Font *font, const char *text, SDL_Color color,
                       SDL_Rect *dest) {
  if (!text)
    text = "";

  SDL_Surface *surface = TTF_RenderUTF8_Blended(font, *text ? text : " ", color);
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

  int text_width;
  TTF_SizeUTF8(font, text, &text_width, NULL);

  SDL_FreeSurface(surface);
  SDL_DestroyTexture(texture);
}

void render_button(Button_t *button) {
  // Draw the filled background
  SDL_SetRenderDrawColor(button->renderer, button->bg_color.r, button->bg_color.g,
                         button->bg_color.b, button->bg_color.a);
  SDL_RenderFillRect(button->renderer, &button->rect);

  // Adjust intensity scale based on hover state
  float lighten_factor = (button->hovered && button->enabled) ? 0.5f : 0.3f;
  float darken_factor = (button->hovered && button->enabled) ? 0.5f : 0.7f;

  // Compute lighter and darker shades of the background color
  Uint8 light_r = button->bg_color.r + (Uint8)((255 - button->bg_color.r) * lighten_factor);
  Uint8 light_g = button->bg_color.g + (Uint8)((255 - button->bg_color.g) * lighten_factor);
  Uint8 light_b = button->bg_color.b + (Uint8)((255 - button->bg_color.b) * lighten_factor);

  Uint8 dark_r = (Uint8)(button->bg_color.r * darken_factor);
  Uint8 dark_g = (Uint8)(button->bg_color.g * darken_factor);
  Uint8 dark_b = (Uint8)(button->bg_color.b * darken_factor);

  // Determine border thickness (6% of smaller dimension, clamped)
  int min_dim = button->rect.w < button->rect.h ? button->rect.w : button->rect.h;
  int border_thickness = SDL_clamp(min_dim / 16, 1, 4);

  // Draw top-left (light) border
  SDL_SetRenderDrawColor(button->renderer, light_r, light_g, light_b, 255);
  for (int i = 0; i < border_thickness; ++i) {
    SDL_RenderDrawLine(button->renderer, button->rect.x, button->rect.y + i,
                       button->rect.x + button->rect.w - 1, button->rect.y + i); // Top
    SDL_RenderDrawLine(button->renderer, button->rect.x + i, button->rect.y, button->rect.x + i,
                       button->rect.y + button->rect.h - 1); // Left
  }

  // Draw bottom-right (dark) border
  SDL_SetRenderDrawColor(button->renderer, dark_r, dark_g, dark_b, 255);
  for (int i = 0; i < border_thickness; ++i) {
    SDL_RenderDrawLine(button->renderer, button->rect.x, button->rect.y + button->rect.h - 1 - i,
                       button->rect.x + button->rect.w - 1,
                       button->rect.y + button->rect.h - 1 - i); // Bottom
    SDL_RenderDrawLine(button->renderer, button->rect.x + button->rect.w - 1 - i, button->rect.y,
                       button->rect.x + button->rect.w - 1 - i,
                       button->rect.y + button->rect.h - 1); // Right
  }

  // Render the text centered on the button
  SDL_Surface *textSurface = TTF_RenderUTF8_Blended(
      button->font, button->text,
      button->enabled == true ? button->fg_color : get_color(COLOR_LIGHTGRAY));
  if (!textSurface)
    return;

  SDL_Texture *textTexture = SDL_CreateTextureFromSurface(button->renderer, textSurface);

  int text_x = button->rect.x + (button->rect.w - textSurface->w) / 2;
  int text_y = button->rect.y + (button->rect.h - textSurface->h) / 2;
  SDL_Rect textRect = {text_x, text_y, textSurface->w, textSurface->h};

  SDL_RenderCopy(button->renderer, textTexture, NULL, &textRect);

  SDL_FreeSurface(textSurface);
  SDL_DestroyTexture(textTexture);
}

void do_sdl_cleanup(ESdlContext_t *sdl_context) {
  SDL_DestroyRenderer(sdl_context->renderer);
  SDL_DestroyWindow(sdl_context->window);
  SDL_Quit();
}
