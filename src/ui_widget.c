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

// void vstack_begin(VStack_t *v, int x, int y, int spacing) {
// v->x = x;
// v->y = y;
// v->spacing = spacing;
// v->current_y = y;
//}

// void vstack_place(VStack_t *v, UIWidget_t *w) {
// w->rect.x = v->x;
// w->rect.y = v->current_y;

// v->current_y += w->rect.h + v->spacing;
//}

// void vstack_place_right(VStack_t *v, UIWidget_t *w, int right_edge) {
// w->rect.x = right_edge - w->rect.w;
// w->rect.y = v->current_y;

// v->current_y += w->rect.h + v->spacing;
//}

void ui_table_begin(UITable_t *t, int x, int y, int cols) {
  memset(t, 0, sizeof(*t));
  t->x = x;
  t->y = y;
  t->cols = cols;
  t->row_spacing = 8;
  t->col_spacing = 20;
}

void ui_table_add(UITable_t *t, int row, int col, UIWidget_t *w) {
  t->cells[row][col] = w;

  if (w->rect.w > t->col_width[col])
    t->col_width[col] = w->rect.w;

  if (w->rect.h > t->row_height[row])
    t->row_height[row] = w->rect.h;

  if (row >= t->rows)
    t->rows = row + 1;
}

void ui_table_layout(UITable_t *t) {
  int y = t->y;

  for (int r = 0; r < t->rows; r++) {

    int x = t->x;

    for (int c = 0; c < t->cols; c++) {

      UIWidget_t *w = t->cells[r][c];
      if (w) {
        w->rect.x = x;
        w->rect.y = y;
      }

      x += t->col_width[c] + t->col_spacing;
    }

    y += t->row_height[r] + t->row_spacing;
  }
}
