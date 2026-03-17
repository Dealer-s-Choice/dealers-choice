#ifndef __UI_WIDGET_H
#define __UI_WIDGET_H

#include <SDL.h>

typedef struct UIWidget_t UIWidget_t;

struct UIWidget_t {
  SDL_Rect rect;

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
  int x;
  int y;
  int spacing;
  int current_y;
} VStack_t;

/* lifecycle helpers */
void ui_widget_render(UIWidget_t *w);
void ui_widget_destroy(UIWidget_t *w);

/* simple layout helper */
void ui_widget_place(UIWidget_t *w, int x, int y);

void vstack_begin(VStack_t *v, int x, int y, int spacing);

void vstack_place(VStack_t *v, UIWidget_t *w);

void vstack_place_right(VStack_t *v, UIWidget_t *w, int right_edge);

void ui_table_begin(UITable_t *t, int x, int y, int cols);

void ui_table_add(UITable_t *t, int row, int col, UIWidget_t *w);

void ui_table_layout(UITable_t *t);
#endif
