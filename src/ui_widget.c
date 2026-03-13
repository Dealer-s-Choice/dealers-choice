#include "ui_widget.h"

void ui_widget_render(UIWidget_t *w) {
  if (!w)
    return;

  if (w->render)
    w->render(w);
}

void ui_widget_destroy(UIWidget_t *w) {
  if (!w)
    return;

  if (w->destroy)
    w->destroy(w);
}

void ui_widget_place(UIWidget_t *w, int x, int y) {
  if (!w)
    return;

  w->rect.x = x;
  w->rect.y = y;
}
