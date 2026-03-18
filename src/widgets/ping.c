#include "ping.h"

static void ping_widget_destroy(UIWidget_t *w) {
  PingWidget_t *pw = (PingWidget_t *)w;

  if (pw->text)
    ui_widget_destroy(&pw->text->base);

  free(pw);
}

static void ping_widget_render(UIWidget_t *w) {
  PingWidget_t *pw = (PingWidget_t *)w;

  if (!pw->text)
    return;

  /* inherit position from base */
  pw->text->base.rect.x = pw->base.rect.x;
  pw->text->base.rect.y = pw->base.rect.y;

  ui_widget_render(&pw->text->base);
}

PingWidget_t *ping_widget_create(int ping, TTF_Font *font, SDL_Color color) {
  PingWidget_t *pw = calloc(1, sizeof(*pw));
  if (!pw)
    return NULL;

  char buf[32];
  snprintf(buf, sizeof buf, "ping %dms", ping);

  pw->text = text_widget_create(buf, font, color);
  pw->ping = ping;

  pw->base.render = ping_widget_render;
  pw->base.destroy = ping_widget_destroy;

  /* size = text size */
  pw->base.rect.w = pw->text->base.rect.w;
  pw->base.rect.h = pw->text->base.rect.h;

  return pw;
}

void ping_widget_update(PingWidget_t *pw, int ping) {
  if (!pw)
    return;

  if (pw->ping == ping)
    return; // no change → no texture rebuild

  pw->ping = ping;

  char buf[32];
  snprintf(buf, sizeof buf, "ping %dms", ping);

  text_widget_set_text(pw->text, buf);
}
