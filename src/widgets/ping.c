#include "ping.h"

static void ping_widget_render(UIWidget_t *w) {
  PingWidget_t *pw = (PingWidget_t *)w;

  if (!pw->text)
    return;

  // Position text using base rect
  pw->text->base.rect.x = w->rect.x;
  pw->text->base.rect.y = w->rect.y;

  ui_widget_render(&pw->text->base);
}

PingWidget_t *ping_widget_create(int ping, TTF_Font *font) {
  PingWidget_t *pw = calloc(1, sizeof(*pw));
  if (!pw)
    return NULL;

  char buf[32];
  snprintf(buf, sizeof buf, "ping %dms", ping);

  pw->text = text_widget_create(buf, font, get_color(COLOR_WHITE));
  pw->ping = ping;

  // IMPORTANT: propagate size to base widget
  pw->base.rect.w = pw->text->base.rect.w;
  pw->base.rect.h = pw->text->base.rect.h;

  // hook render/destroy
  pw->base.render = ping_widget_render;
  pw->base.destroy = text_wrapper_destroy;

  return pw;
}

void ping_widget_update(PingWidget_t *pw, int ping) {
  if (pw->ping == ping)
    return;
  pw->ping = ping;
  char buf[32];
  snprintf(buf, sizeof buf, "ping %dms", ping);
  text_widget_set_text(pw->text, buf);

  pw->base.rect.w = pw->text->base.rect.w;
  pw->base.rect.h = pw->text->base.rect.h;
}
