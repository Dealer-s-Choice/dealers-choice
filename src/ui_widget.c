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

void ui_register(UIRegistry_t *reg, UIWidget_t *w) {
  if (!reg || !w)
    return;

  if (reg->count >= MAX_WIDGETS)
    return;

  w->enabled = true;
  reg->items[reg->count++] = w;
}

/*
 * Remove a widget from the registry before destroying it.
 * Always call this before ui_widget_destroy() on any registered widget —
 * the registry holds raw pointers and does not know when a widget is freed.
 * Failing to unregister first will cause ui_render_all() to call render()
 * on freed memory (heap-use-after-free).
 *
 * Uses swap-with-last to remove in O(1). Order is not preserved.
 *
 * Comment written by Claude (Anthropic).
 */
void ui_unregister(UIRegistry_t *reg, UIWidget_t *w) {
  if (!reg || !w)
    return;
  for (int i = 0; i < reg->count; i++) {
    if (reg->items[i] == w) {
      reg->items[i] = reg->items[--reg->count];
      reg->items[reg->count] = NULL;
      return;
    }
  }
}

void ui_destroy_all(UIRegistry_t *reg) {
  if (!reg)
    return;

  for (int i = 0; i < reg->count; i++) {
    if (reg->items[i])
      ui_widget_destroy(reg->items[i]);
  }

  reg->count = 0;
}

void ui_render_all(UIRegistry_t *reg) {
  if (!reg)
    return;
  for (int i = 0; i < reg->count; i++) {
    if (reg->items[i] && reg->items[i]->enabled)
      ui_widget_render(reg->items[i]);
  }
}

void ui_widget_place(UIWidget_t *w, int x, int y) {
  if (!w)
    return;

  w->rect.x = x;
  w->rect.y = y;
}

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

void ui_table_draw_row_separators(const UITable_t *t, SDL_Renderer *renderer) {
  if (!t || !renderer || t->rows < 2)
    return;

  int max_x = 0;
  for (int r = 0; r < t->rows; r++) {
    for (int c = 0; c < t->cols; c++) {
      UIWidget_t *w = t->cells[r][c];
      if (w && w->rect.x + w->rect.w > max_x)
        max_x = w->rect.x + w->rect.w;
    }
  }

  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);

  int y = t->y;
  for (int r = 0; r < t->rows - 1; r++) {
    int line_y = y + t->row_height[r] + t->row_spacing / 2;
    SDL_Rect sep = {t->x, line_y, max_x - t->x, 2};
    SDL_RenderFillRect(renderer, &sep);
    y += t->row_height[r] + t->row_spacing;
  }
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
