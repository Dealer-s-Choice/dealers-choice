#ifndef __UI_WIDGET_H
#define __UI_WIDGET_H

#include "graphics.h"
#include <stdbool.h>

#define MAX_WIDGETS 128

typedef struct UIWidget_t UIWidget_t;

struct UIWidget_t {
  SDL_Rect rect;
  bool hovered;

  void (*render)(UIWidget_t *w);
  void (*destroy)(UIWidget_t *w);
};

typedef struct {
  int x;
  int y;

  int rows;
  int cols;

  int row_spacing;
  int col_spacing;

  int col_width[8];
  int row_height[32];

  UIWidget_t *cells[32][8];
} UITable_t;

typedef struct {
  UIWidget_t *items[MAX_WIDGETS];
  int count;
} UIRegistry_t;

/* lifecycle helpers */
void ui_widget_render(UIWidget_t *w);
void ui_widget_destroy(UIWidget_t *w);

/* simple layout helper */
void ui_widget_place(UIWidget_t *w, int x, int y);

void ui_table_begin(UITable_t *t, int x, int y, int cols);

void ui_table_add(UITable_t *t, int row, int col, UIWidget_t *w);

void ui_table_layout(UITable_t *t);

void ui_register(UIRegistry_t *reg, UIWidget_t *w);

void ui_destroy_all(UIRegistry_t *reg);

void ui_render_all(UIRegistry_t *reg);
#endif
