/*
 player_widget.c
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

#include "player_widget.h"
#include "translate.h"

PlayerWidget_t *player_widget_create(const char *nick, bool dealer, uint32_t ping, TTF_Font *font) {
  SDL_Renderer *renderer = g_sdl_context->renderer;

  PlayerWidget_t *pw = calloc(1, sizeof(*pw));
  if (!pw)
    return NULL;

  pw->renderer = renderer;
  pw->font = font;
  pw->color = get_color(COLOR_WHITE);

  char nick_buf[SIZEOF_NICK + 32];
  snprintf(nick_buf, sizeof nick_buf, "%s%s", nick, dealer ? _(" (Dealer)") : "");

  char ping_buf[32];
  snprintf(ping_buf, sizeof ping_buf, "ping %ums", ping);

  SDL_Surface *s;

  /* nickname */
  s = TTF_RenderUTF8_Blended(font, nick_buf, pw->color);
  pw->nick_tex = SDL_CreateTextureFromSurface(renderer, s);
  pw->nick_rect.w = s->w;
  pw->nick_rect.h = s->h;
  SDL_FreeSurface(s);

  /* ping */
  s = TTF_RenderUTF8_Blended(font, ping_buf, pw->color);
  pw->ping_tex = SDL_CreateTextureFromSurface(renderer, s);
  pw->ping_rect.w = s->w;
  pw->ping_rect.h = s->h;
  SDL_FreeSurface(s);

  /* IMPORTANT: widget size for table layout */
  int spacing = 20;

  pw->base.rect.w = pw->nick_rect.w + spacing + pw->ping_rect.w;
  pw->base.rect.h = SDL_max(pw->nick_rect.h, pw->ping_rect.h);

  pw->ping = ping;

  pw->base.destroy = player_widget_destroy;
  pw->base.render = player_widget_render;

  return pw;
}

void player_widget_update_ping(PlayerWidget_t *pw, int ping) {
  if (!pw)
    return;

  if (pw->ping == ping)
    return; // no change

  pw->ping = ping;

  char buf[32];
  snprintf(buf, sizeof buf, "ping %dms", ping);

  if (pw->ping_tex) {
    SDL_DestroyTexture(pw->ping_tex);
    pw->ping_tex = NULL;
  }

  SDL_Surface *surf = TTF_RenderUTF8_Blended(pw->font, buf, pw->color);
  if (!surf)
    return;

  pw->ping_tex = SDL_CreateTextureFromSurface(pw->renderer, surf);

  pw->ping_rect.w = surf->w;
  pw->ping_rect.h = surf->h;

  SDL_FreeSurface(surf);
}

void player_widget_layout(PlayerWidget_t *pw) {
  pw->nick_rect.x = pw->base.rect.x;
  pw->nick_rect.y = pw->base.rect.y;

  pw->ping_rect.x = pw->base.rect.x;
  pw->ping_rect.y = pw->base.rect.y;
}

void player_widget_render(UIWidget_t *w) {
  PlayerWidget_t *pw = (PlayerWidget_t *)w;

  int spacing = 20;

  pw->nick_rect.x = pw->base.rect.x;
  pw->nick_rect.y = pw->base.rect.y;

  pw->ping_rect.x = pw->base.rect.x + pw->nick_rect.w + spacing;
  pw->ping_rect.y = pw->base.rect.y;

  SDL_RenderCopy(pw->renderer, pw->nick_tex, NULL, &pw->nick_rect);
  SDL_RenderCopy(pw->renderer, pw->ping_tex, NULL, &pw->ping_rect);
}

void player_widget_destroy(UIWidget_t *w) {
  if (!w)
    return;

  PlayerWidget_t *pw = (PlayerWidget_t *)w;

  if (pw->nick_tex)
    SDL_DestroyTexture(pw->nick_tex);

  if (pw->ping_tex)
    SDL_DestroyTexture(pw->ping_tex);

  free(pw);
}
