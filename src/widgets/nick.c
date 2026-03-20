#include "nick.h"
#include "globals.h"

static void nick_widget_render(UIWidget_t *w) {
  NickWidget_t *nw = (NickWidget_t *)w;
  if (!nw->text)
    return;
  if (nw->highlight || nw->selectable) {
    SDL_Renderer *r = g_sdl_context->renderer;
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    if (nw->selectable && w->selected)
      SDL_SetRenderDrawColor(r, 20, 20, 100, 220);
    else if (nw->selectable && w->hovered)
      SDL_SetRenderDrawColor(r, 0, 0, 0, 180);
    else if (nw->highlight)
      SDL_SetRenderDrawColor(r, 0, 0, 0, 128);
    else
      goto skip_bg;
    SDL_RenderFillRect(r, &w->rect);
  skip_bg:
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
  }
  nw->text->base.rect.x = w->rect.x;
  nw->text->base.rect.y = w->rect.y;
  ui_widget_render(&nw->text->base);
}

NickWidget_t *nick_widget_create(const char *nick, const int8_t id, TTF_Font *font) {
  NickWidget_t *nw = calloc(1, sizeof(*nw));
  if (!nw)
    return NULL;
  nw->id = id;
  nw->text = text_widget_create(nick, font, get_color(COLOR_WHITE));
  if (!nw->text) {
    free(nw);
    return NULL;
  }
  nw->base.rect.w = nw->text->base.rect.w;
  nw->base.rect.h = nw->text->base.rect.h;
  nw->base.render = nick_widget_render;
  nw->base.destroy = text_wrapper_destroy;
  return nw;
}
