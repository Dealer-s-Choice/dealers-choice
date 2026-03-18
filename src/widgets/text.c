#include "text.h"
#include "util.h"

static void text_widget_render(UIWidget_t *w) {
  TextWidget_t *tw = (TextWidget_t *)w;

  if (!tw->tex)
    return;

  SDL_Rect dst = {w->rect.x, w->rect.y, tw->rect.w, tw->rect.h};

  SDL_RenderCopy(tw->renderer, tw->tex, NULL, &dst);
}

static void text_widget_destroy(UIWidget_t *w) {
  TextWidget_t *tw = (TextWidget_t *)w;

  if (tw->tex)
    SDL_DestroyTexture(tw->tex);

  free(tw->text);
  free(tw);
}

TextWidget_t *text_widget_create(const char *text, TTF_Font *font, SDL_Color color) {
  SDL_Renderer *renderer = g_sdl_context->renderer;
  if (!renderer || !text || !font)
    return NULL;

  TextWidget_t *tw = calloc(1, sizeof(*tw));
  if (!tw)
    return NULL;

  tw->renderer = renderer;
  tw->font = font;
  tw->color = color;
  tw->text = dc_strdup(text);

  SDL_Surface *s = TTF_RenderUTF8_Blended(font, text, color);
  tw->tex = SDL_CreateTextureFromSurface(renderer, s);

  tw->rect.w = s->w;
  tw->rect.h = s->h;

  /* important: widget size */
  tw->base.rect.w = s->w;
  tw->base.rect.h = s->h;

  SDL_FreeSurface(s);

  tw->base.render = text_widget_render;
  tw->base.destroy = text_widget_destroy;

  return tw;
}

void text_widget_set_text(TextWidget_t *tw, const char *text) {
  if (!tw || !text)
    return;

  if (tw->tex)
    SDL_DestroyTexture(tw->tex);

  free(tw->text);
  tw->text = dc_strdup(text);

  SDL_Surface *s = TTF_RenderUTF8_Blended(tw->font, text, tw->color);
  tw->tex = SDL_CreateTextureFromSurface(tw->renderer, s);

  tw->rect.w = s->w;
  tw->rect.h = s->h;

  tw->base.rect.w = s->w;
  tw->base.rect.h = s->h;

  SDL_FreeSurface(s);
}
