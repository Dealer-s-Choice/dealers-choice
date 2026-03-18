#include "nick.h"

static void nick_widget_render(UIWidget_t *w) {
  NickWidget_t *nw = (NickWidget_t *)w;
  if (!nw->text)
    return;
  nw->text->base.rect.x = w->rect.x;
  nw->text->base.rect.y = w->rect.y;
  ui_widget_render(&nw->text->base);
}

static void nick_widget_destroy(UIWidget_t *w) {
  NickWidget_t *nw = (NickWidget_t *)w;
  if (nw->text)
    ui_widget_destroy(&nw->text->base);
  free(nw);
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
  nw->base.destroy = nick_widget_destroy;
  return nw;
}
