#include "button.h"
#include "globals.h"

static float clicked_progress(const Clicked_t *c) {
  if (!c || c->start_time == 0)
    return 0.0f;

  uint32_t now = SDL_GetTicks();
  float t = (now - c->start_time) / (float)c->duration;
  return t >= 1.0f ? 1.0f : t;
}

static void button_widget_render(UIWidget_t *w) {
  ButtonWidget_t *bw = (ButtonWidget_t *)w;
  SDL_Renderer *r = g_sdl_context->renderer;

  float click_t = clicked_progress(&bw->click);
  if (click_t >= 1.0f) {
    bw->click.start_time = 0;
    click_t = 0.0f;
  }

  int press_offset = (int)(click_t * 8);
  SDL_Rect rect = w->rect;
  rect.y += press_offset;

  SDL_SetRenderDrawColor(r, bw->color.bg.r, bw->color.bg.g, bw->color.bg.b, bw->color.bg.a);
  SDL_RenderFillRect(r, &rect);

  float lighten = (bw->interactive && w->hovered) ? 0.7f : 0.3f;
  float darken = (bw->interactive && w->hovered) ? 0.5f : 0.9f;

  Uint8 light_r = bw->color.bg.r + (Uint8)((255 - bw->color.bg.r) * lighten);
  Uint8 light_g = bw->color.bg.g + (Uint8)((255 - bw->color.bg.g) * lighten);
  Uint8 light_b = bw->color.bg.b + (Uint8)((255 - bw->color.bg.b) * lighten);

  Uint8 dark_r = (Uint8)(bw->color.bg.r * darken);
  Uint8 dark_g = (Uint8)(bw->color.bg.g * darken);
  Uint8 dark_b = (Uint8)(bw->color.bg.b * darken);

  int min_dim = rect.w < rect.h ? rect.w : rect.h;
  int border = SDL_clamp(min_dim / 16, 1, 4);

  SDL_SetRenderDrawColor(r, light_r, light_g, light_b, 255);
  for (int i = 0; i < border; i++) {
    SDL_RenderDrawLine(r, rect.x, rect.y + i, rect.x + rect.w - 1, rect.y + i);
    SDL_RenderDrawLine(r, rect.x + i, rect.y, rect.x + i, rect.y + rect.h - 1);
  }

  if (bw->base.selected)
    mark_selected(r, &rect);

  SDL_SetRenderDrawColor(r, dark_r, dark_g, dark_b, 255);
  for (int i = 0; i < border; i++) {
    SDL_RenderDrawLine(r, rect.x, rect.y + rect.h - 1 - i, rect.x + rect.w - 1,
                       rect.y + rect.h - 1 - i);
    SDL_RenderDrawLine(r, rect.x + rect.w - 1 - i, rect.y, rect.x + rect.w - 1 - i,
                       rect.y + rect.h - 1);
  }

  TextWidget_t *tw = bw->interactive ? bw->text : bw->text_disabled;
  if (tw) {
    tw->base.rect.x = rect.x + (rect.w - tw->base.rect.w) / 2;
    tw->base.rect.y = rect.y + (rect.h - tw->base.rect.h) / 2;
    ui_widget_render(&tw->base);
  }
}

static void button_widget_destroy(UIWidget_t *w) {
  ButtonWidget_t *bw = (ButtonWidget_t *)w;
  if (bw->text)
    ui_widget_destroy(&bw->text->base);
  if (bw->text_disabled)
    ui_widget_destroy(&bw->text_disabled->base);
  free(bw);
}

ButtonWidget_t *button_widget_create(const char *text, EColor_t color, TTF_Font *font,
                                     SDL_Keycode hotkey) {
  ButtonWidget_t *bw = calloc(1, sizeof(*bw));
  if (!bw)
    return NULL;

  bw->color = (Color_t){get_color(color.bg), get_color(color.fg)};
  bw->hotkey = hotkey;
  bw->click.duration = 80;
  bw->interactive = true;

  bw->text = text_widget_create(text, font, get_color(color.fg));
  bw->text_disabled = text_widget_create(text, font, get_color(COLOR_LIGHTGRAY));

  if (!bw->text || !bw->text_disabled) {
    button_widget_destroy(&bw->base);
    return NULL;
  }

  int tw, th;
  if (TTF_SizeUTF8(font, text, &tw, &th) != 0) {
    tw = 40;
    th = 20;
  }

  bw->base.rect.w = tw + 20;
  bw->base.rect.h = th + 10;
  bw->base.render = button_widget_render;
  bw->base.destroy = button_widget_destroy;

  return bw;
}
