#include <stdlib.h>

#include "globals_gui.h"
#include "graphics.h"
#include "round_button.h"

static Uint8 clampb(int v) { return v < 0 ? 0 : (v > 255 ? 255 : (Uint8)v); }

static void round_button_render(UIWidget_t *w) {
  RoundButtonWidget_t *b = (RoundButtonWidget_t *)w;
  SDL_Renderer *r = g_sdl_context->renderer;
  const int cx = w->rect.x + w->rect.w / 2;
  const int cy = w->rect.y + w->rect.h / 2;
  const int rad = w->rect.w / 2;
  const int boost = w->hovered ? 45 : 0;
  const SDL_Color c = {clampb(b->color.r + boost), clampb(b->color.g + boost),
                       clampb(b->color.b + boost), 255};

  /* dark rim/shadow, then the body, then an off-center specular highlight to
   * read as a lit sphere */
  SDL_SetRenderDrawColor(r, clampb(c.r / 3), clampb(c.g / 3), clampb(c.b / 3), 255);
  gfx_fill_circle(r, cx, cy, rad);
  SDL_SetRenderDrawColor(r, c.r, c.g, c.b, 255);
  gfx_fill_circle(r, cx, cy, rad - 2);
  SDL_SetRenderDrawColor(r, clampb(c.r + 90), clampb(c.g + 90), clampb(c.b + 90), 255);
  gfx_fill_circle(r, cx - rad / 4, cy - rad / 4, rad / 3);
}

static void round_button_destroy(UIWidget_t *w) { free(w); }

RoundButtonWidget_t *round_button_create(int diameter, SDL_Color color) {
  RoundButtonWidget_t *b = calloc(1, sizeof(*b));
  if (!b)
    return NULL;
  b->color = color;
  b->base.rect.w = diameter;
  b->base.rect.h = diameter;
  b->base.render = round_button_render;
  b->base.destroy = round_button_destroy;
  return b;
}
