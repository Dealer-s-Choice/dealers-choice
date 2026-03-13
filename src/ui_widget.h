#ifndef __UI_WIDGET_H
#define __UI_WIDGET_H

#include <SDL.h>

typedef struct UIWidget_t UIWidget_t;

struct UIWidget_t {
  SDL_Rect rect;

  void (*render)(UIWidget_t *w);
  void (*destroy)(UIWidget_t *w);
};

/* lifecycle helpers */
void ui_widget_render(UIWidget_t *w);
void ui_widget_destroy(UIWidget_t *w);

/* simple layout helper */
void ui_widget_place(UIWidget_t *w, int x, int y);

#endif
