/*
 button.c
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

#include "button.h"
#include "game.h"

static float clicked_progress(const Clicked_t *c) {
  if (!c || c->start_time == 0)
    return 0.0f;

  uint32_t now = SDL_GetTicks();
  float t = (now - c->start_time) / (float)c->duration;

  if (t >= 1.0f)
    return 1.0f;

  return t;
}

void render_button(Button_t *button) {
  if (!button->active)
    return;

  float click_t = clicked_progress(&button->click);

  if (click_t >= 1.0f) {
    button->click.start_time = 0; // animation finished
    click_t = 0.0f;
  }

  int press_offset = (int)(click_t * 8);
  SDL_Rect rect = button->rect;
  rect.y += press_offset;

  // Draw the filled background
  SDL_SetRenderDrawColor(button->renderer, button->color.bg.r, button->color.bg.g,
                         button->color.bg.b, button->color.bg.a);
  SDL_RenderFillRect(button->renderer, &rect);

  // Adjust intensity scale based on hover state
  float lighten_factor = (button->hovered && button->enabled) ? 0.7f : 0.3f;
  float darken_factor = (button->hovered && button->enabled) ? 0.5f : 0.9f;

  // Compute lighter and darker shades of the background color
  Uint8 light_r = button->color.bg.r + (Uint8)((255 - button->color.bg.r) * lighten_factor);
  Uint8 light_g = button->color.bg.g + (Uint8)((255 - button->color.bg.g) * lighten_factor);
  Uint8 light_b = button->color.bg.b + (Uint8)((255 - button->color.bg.b) * lighten_factor);

  Uint8 dark_r = (Uint8)(button->color.bg.r * darken_factor);
  Uint8 dark_g = (Uint8)(button->color.bg.g * darken_factor);
  Uint8 dark_b = (Uint8)(button->color.bg.b * darken_factor);

  // Determine border thickness (6% of smaller dimension, clamped)
  int min_dim = rect.w < rect.h ? rect.w : rect.h;
  int border_thickness = SDL_clamp(min_dim / 16, 1, 4);

  // Draw top-left (light) border
  SDL_SetRenderDrawColor(button->renderer, light_r, light_g, light_b, 255);
  for (int i = 0; i < border_thickness; ++i) {
    SDL_RenderDrawLine(button->renderer, rect.x, rect.y + i, rect.x + rect.w - 1,
                       rect.y + i); // Top
    SDL_RenderDrawLine(button->renderer, rect.x + i, rect.y, rect.x + i,
                       rect.y + rect.h - 1); // Left
  }

  if (button->selected)
    mark_selected(button->renderer, &rect);

  // Draw bottom-right (dark) border
  SDL_SetRenderDrawColor(button->renderer, dark_r, dark_g, dark_b, 255);
  for (int i = 0; i < border_thickness; ++i) {
    SDL_RenderDrawLine(button->renderer, rect.x, rect.y + rect.h - 1 - i, rect.x + rect.w - 1,
                       rect.y + rect.h - 1 - i); // Bottom
    SDL_RenderDrawLine(button->renderer, rect.x + rect.w - 1 - i, rect.y, rect.x + rect.w - 1 - i,
                       rect.y + rect.h - 1); // Right
  }

  // Render the text centered on the button
  SDL_Surface *textSurface = TTF_RenderUTF8_Blended(
      button->font, button->text,
      button->enabled == true ? button->color.fg : get_color(COLOR_LIGHTGRAY));
  if (!textSurface)
    return;

  SDL_Texture *textTexture = SDL_CreateTextureFromSurface(button->renderer, textSurface);

  int text_x = rect.x + (rect.w - textSurface->w) / 2;
  int text_y = rect.y + (rect.h - textSurface->h) / 2;
  SDL_Rect textRect = {text_x, text_y, textSurface->w, textSurface->h};

  SDL_RenderCopy(button->renderer, textTexture, NULL, &textRect);

  SDL_FreeSurface(textSurface);
  SDL_DestroyTexture(textTexture);
}

Button_t create_button(const char *text, EColor_t color, TTF_Font *font, const SDL_Keycode key) {
  Button_t b = {
      .text = text,
      .renderer = g_sdl_context->renderer,
      .color = {get_color(color.bg), get_color(color.fg)},
      .rect = (SDL_Rect){0},
      .font = font,
      .hovered = false,
      .enabled = true,
      .selected = false,
      .active = true,
      .hotkey = key,
      CLICKED_DEFAULT,
  };

  if (TTF_SizeUTF8(font, text, &b.rect.w, &b.rect.h) != 0) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "TTF_SizeUTF8 failed: %s", TTF_GetError());

    /* fallback size */
    b.rect.w = 40;
    b.rect.h = 20;
  }

  b.rect.w += 20;
  b.rect.h += 10;
  return b;
}
