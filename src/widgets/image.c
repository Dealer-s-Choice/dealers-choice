#include "image.h"
#include "globals.h"

static void image_widget_render(UIWidget_t *w) {
  ImageWidget_t *iw = (ImageWidget_t *)w;
  if (!iw->tex)
    return;
  SDL_SetTextureAlphaMod(iw->tex, w->hovered ? 160 : 255);
  SDL_RenderCopy(iw->renderer, iw->tex, NULL, &w->rect);
}

static void image_widget_destroy(UIWidget_t *w) {
  ImageWidget_t *iw = (ImageWidget_t *)w;
  if (iw->tex)
    SDL_DestroyTexture(iw->tex);
  free(iw);
}

static void image_widget_destroy_no_tex(UIWidget_t *w) { free(w); }

ImageWidget_t *image_widget_from_texture(SDL_Texture *tex, int w, int h) {
  if (!tex)
    return NULL;

  ImageWidget_t *iw = calloc(1, sizeof(*iw));
  if (!iw)
    return NULL;

  iw->renderer = g_sdl_context->renderer;
  iw->tex = tex;
  iw->base.rect.w = w;
  iw->base.rect.h = h;
  iw->base.render = image_widget_render;
  iw->base.destroy = image_widget_destroy_no_tex;
  return iw;
}

ImageWidget_t *image_widget_create(const char *path, int w, int h) {
  SDL_Renderer *renderer = g_sdl_context->renderer;
  if (!renderer || !path)
    return NULL;

  SDL_Texture *tex = load_texture(renderer, path);
  if (!tex)
    return NULL;

  ImageWidget_t *iw = calloc(1, sizeof(*iw));
  if (!iw) {
    SDL_DestroyTexture(tex);
    return NULL;
  }

  iw->renderer = renderer;
  iw->tex = tex;

  if (w > 0 && h > 0) {
    iw->base.rect.w = w;
    iw->base.rect.h = h;
  } else {
    SDL_QueryTexture(tex, NULL, NULL, &iw->base.rect.w, &iw->base.rect.h);
  }

  iw->base.render = image_widget_render;
  iw->base.destroy = image_widget_destroy;

  return iw;
}
