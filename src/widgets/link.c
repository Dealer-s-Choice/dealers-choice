#include "link.h"
#include "globals.h"
#include "graphics.h"

#define LINK_PAD_X 10
#define LINK_PAD_Y 2

static void link_widget_render(UIWidget_t *w) {
  LinkWidget_t *lw = (LinkWidget_t *)w;
  SDL_Renderer *r = g_sdl_context->renderer;

  if (w->hovered)
    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
  else
    SDL_SetRenderDrawColor(r, 230, 245, 230, 255);
  SDL_RenderFillRect(r, &w->rect);

  TextWidget_t *tw = w->hovered ? lw->text_hovered : lw->text_normal;
  if (tw) {
    tw->base.rect.x = w->rect.x + LINK_PAD_X;
    tw->base.rect.y = w->rect.y + LINK_PAD_Y;
    ui_widget_render(&tw->base);
  }
}

static void link_widget_destroy(UIWidget_t *w) {
  LinkWidget_t *lw = (LinkWidget_t *)w;
  if (lw->text_normal)
    ui_widget_destroy(&lw->text_normal->base);
  if (lw->text_hovered)
    ui_widget_destroy(&lw->text_hovered->base);
  free(lw);
}

LinkWidget_t *link_widget_create(const char *text, const char *url, TTF_Font *font) {
  if (!text || !url || !font)
    return NULL;

  LinkWidget_t *lw = calloc(1, sizeof(*lw));
  if (!lw)
    return NULL;

  lw->url = url;

  TTF_SetFontStyle(font, TTF_STYLE_UNDERLINE);
  lw->text_normal = text_widget_create(text, font, get_color(COLOR_BLACK));
  lw->text_hovered = text_widget_create(text, font, get_color(COLOR_BLUE));
  TTF_SetFontStyle(font, TTF_STYLE_NORMAL);

  if (!lw->text_normal || !lw->text_hovered) {
    link_widget_destroy(&lw->base);
    return NULL;
  }

  lw->base.rect.w = lw->text_normal->base.rect.w + LINK_PAD_X * 2;
  lw->base.rect.h = lw->text_normal->base.rect.h + LINK_PAD_Y * 2;

  lw->base.render = link_widget_render;
  lw->base.destroy = link_widget_destroy;

  return lw;
}
