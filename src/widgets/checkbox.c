#include "checkbox.h"
#include "../graphics.h"
#include "globals_gui.h"

static void checkbox_widget_render(UIWidget_t *w) {
  CheckboxWidget_t *cb = (CheckboxWidget_t *)w;
  SDL_Renderer *r = cb->renderer;

  /* Background */
  SDL_SetRenderDrawColor(r, 30, 30, 30, 255);
  SDL_RenderFillRect(r, &w->rect);

  /* Border: yellow when hovered, white otherwise */
  SDL_Color border = w->hovered ? (SDL_Color){255, 220, 0, 255} : (SDL_Color){200, 200, 200, 255};
  SDL_SetRenderDrawColor(r, border.r, border.g, border.b, border.a);
  draw_rect_border(r, w->rect);

  if (cb->checked) {
    /* Filled inner box in yellow to indicate checked */
    const int pad = w->rect.w / 5;
    SDL_Rect inner = {
        w->rect.x + pad,
        w->rect.y + pad,
        w->rect.w - pad * 2,
        w->rect.h - pad * 2,
    };
    SDL_SetRenderDrawColor(r, 255, 220, 0, 255);
    SDL_RenderFillRect(r, &inner);
  }
}

static void checkbox_widget_destroy(UIWidget_t *w) { free(w); }

CheckboxWidget_t *checkbox_widget_create(bool checked, int size) {
  SDL_Renderer *renderer = g_sdl_context->renderer;
  if (!renderer)
    return NULL;

  CheckboxWidget_t *cb = calloc(1, sizeof(*cb));
  if (!cb)
    return NULL;

  cb->renderer = renderer;
  cb->checked = checked;
  cb->base.rect.w = size;
  cb->base.rect.h = size;
  cb->base.render = checkbox_widget_render;
  cb->base.destroy = checkbox_widget_destroy;

  return cb;
}
