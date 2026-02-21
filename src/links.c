#include <SDL2/SDL_ttf.h>
#include <stddef.h>

#include "config.h"
#include "globals.h"
#include "links.h"
#include "translate.h"
#include "util.h"

const LinkDef_t LINK_DEFS[] = {
    /* TRANSLATORS: "Discord", "Lazarus Project" should not be translated */
    {N_("Discord Channel (on Lazarus Project Server)"),
     "https://discord.com/channels/1295630985429516299/1385298664192217138"},
    {"Matrix", "https://matrix.to/#/#dealers-choice:matrix.org"},
    {N_("Website"), DEALERSCHOICE_URL}};

const size_t LINK_DEFS_COUNT = ARRAY_SIZE(LINK_DEFS);

void init_links(Link_t *link, TTF_Font *font) {
  for (size_t i = 0; i < LINK_DEFS_COUNT; i++) {
    link[i] = (Link_t){.text = LINK_DEFS[i].text,
                       .url = LINK_DEFS[i].url,
                       .font = font,
                       .renderer = g_sdl_context->renderer,
                       .rect = (SDL_Rect){0},
                       .hovered = false};
    if (TTF_SizeUTF8(link[i].font, link[i].text, &link[i].rect.w, &link[i].rect.h) != 0)
      fprintf(stderr, "TTF_SizeUTF8 error: %s\n", TTF_GetError());
  }
}

void render_link(Link_t *link) {
  const uint8_t LINK_PAD_X = 10;
  const uint8_t LINK_PAD_Y = 2;

  TTF_SetFontStyle(link->font, TTF_STYLE_UNDERLINE);

  SDL_Color text_color = link->hovered ? get_color(COLOR_BLUE) : get_color(COLOR_BLACK);

  SDL_Surface *surface = TTF_RenderText_Solid(link->font, link->text, text_color);
  if (!surface) {
    SDL_Log("Failed to render text surface: %s", TTF_GetError());
    return;
  }

  SDL_Texture *texture = SDL_CreateTextureFromSurface(link->renderer, surface);
  if (!texture) {
    SDL_Log("Failed to create texture from surface: %s", SDL_GetError());
    SDL_FreeSurface(surface);
    return;
  }

  /* background rect (with padding) */
  SDL_Rect bg = {link->rect.x, link->rect.y, surface->w + LINK_PAD_X * 2,
                 surface->h + LINK_PAD_Y * 2};

  /* text rect (inside padding) */
  SDL_Rect text_rect = {bg.x + LINK_PAD_X, bg.y + LINK_PAD_Y, surface->w, surface->h};

  SDL_FreeSurface(surface);

  /* background */
  if (link->hovered)
    SDL_SetRenderDrawColor(link->renderer, 255, 255, 255, 255);
  else
    SDL_SetRenderDrawColor(link->renderer, 230, 245, 230, 255);

  SDL_RenderFillRect(link->renderer, &bg);

  /* text */
  SDL_RenderCopy(link->renderer, texture, NULL, &text_rect);
  SDL_DestroyTexture(texture);

  TTF_SetFontStyle(link->font, TTF_STYLE_NORMAL);
}

void layout_links(Link_t *link, size_t count) {
  int center_x = g_sdl_context->win_center.x + SCALE_X(200);

  for (size_t i = 0; i < count; i++) {
    link[i].rect.x = center_x - (link[i].rect.w / 2);

    link[i].rect.y = (g_sdl_context->window_height - (link[i].rect.h * 2)) - (i * link[i].rect.h) -
                     (i * (link[i].rect.h * 0.4));
  }
}
