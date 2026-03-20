#include <math.h>

#include "dealer.h"
#include "globals.h"

static void draw_filled_circle(SDL_Renderer *r, int cx, int cy, int radius) {
  for (int y = -radius; y <= radius; y++) {
    float dy = (float)y / (float)radius;
    float inside = 1.0f - dy * dy;
    if (inside < 0.0f)
      continue;
    int x = (int)(radius * sqrtf(inside) + 0.5f);
    SDL_RenderDrawLine(r, cx - x, cy + y, cx + x, cy + y);
  }
}

static void dealer_widget_render(UIWidget_t *w) {
  DealerWidget_t *dw = (DealerWidget_t *)w;
  SDL_Renderer *r = g_sdl_context->renderer;

  SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
  SDL_RenderFillRect(r, &w->rect);

  if (dw->is_dealer) {
    int cx = w->rect.x + w->rect.w / 2;
    int cy = w->rect.y + w->rect.h / 2;
    int radius = w->rect.w / 3;
    SDL_SetRenderDrawColor(r, 255, 0, 0, 255);
    draw_filled_circle(r, cx, cy, radius);
  }
}

static void dealer_widget_destroy(UIWidget_t *w) { free(w); }

DealerWidget_t *dealer_widget_create(bool is_dealer) {
  DealerWidget_t *dw = calloc(1, sizeof(*dw));
  if (!dw)
    return NULL;

  dw->is_dealer = is_dealer;
  dw->base.rect.w = DEALER_WIDGET_SIZE;
  dw->base.rect.h = DEALER_WIDGET_SIZE;
  dw->base.render = dealer_widget_render;
  dw->base.destroy = dealer_widget_destroy;

  return dw;
}

void dealer_widget_set(DealerWidget_t *dw, bool is_dealer) {
  if (!dw)
    return;
  dw->is_dealer = is_dealer;
}
