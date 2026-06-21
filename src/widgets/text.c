#include "text.h"
#include "util.h"

/* Defensive cap: a malformed/unterminated string must never drive the
 * TTF_RenderUTF8_Blended surface (w*h*4) allocation off into the weeds. */
#define TEXT_WIDGET_MAX_LEN 4096

/* True if text has no NUL within the first TEXT_WIDGET_MAX_LEN bytes
 * (no strnlen: not portable to all DC targets, see lan_discovery.c). */
static bool text_too_long(const char *text) {
  size_t i = 0;
  while (i < TEXT_WIDGET_MAX_LEN && text[i] != '\0')
    i++;
  return i >= TEXT_WIDGET_MAX_LEN;
}

static void text_widget_render(UIWidget_t *w) {
  TextWidget_t *tw = (TextWidget_t *)w;

  if (!tw->tex)
    return;

  SDL_RenderCopy(tw->renderer, tw->tex, NULL, &w->rect); // use base rect directly
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

  if (text_too_long(text)) {
    fprintf(stderr, "text_widget_create: text too long, refusing render\n");
    return NULL;
  }

  TextWidget_t *tw = calloc(1, sizeof(*tw));
  if (!tw)
    return NULL;

  tw->renderer = renderer;
  tw->font = font;
  tw->color = color;
  tw->text = dc_strdup(text);

  /* An empty string has zero width; SDL_ttf returns NULL with a "zero width"
   * error on it. Render nothing (no texture, zero-size rect from calloc) rather
   * than log-spam — text_widget_render already no-ops when tex is NULL. */
  if (text[0] == '\0') {
    tw->base.render = text_widget_render;
    tw->base.destroy = text_widget_destroy;
    return tw;
  }

  SDL_Surface *s = TTF_RenderUTF8_Blended(font, text, color);
  if (!s) {
    fprintf(stderr, "TTF_RenderUTF8_Blended error: %s\n", TTF_GetError());
    tw->base.render = text_widget_render;
    tw->base.destroy = text_widget_destroy;
    return tw;
  }
  tw->tex = SDL_CreateTextureFromSurface(renderer, s);

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

  if (text_too_long(text)) {
    fprintf(stderr, "text_widget_set_text: text too long, refusing render\n");
    return;
  }

  if (tw->tex)
    SDL_DestroyTexture(tw->tex);

  free(tw->text);
  tw->text = dc_strdup(text);

  /* Empty string -> no texture, zero-size rect (see text_widget_create). */
  if (text[0] == '\0') {
    tw->tex = NULL;
    tw->base.rect.w = 0;
    tw->base.rect.h = 0;
    return;
  }

  SDL_Surface *s = TTF_RenderUTF8_Blended(tw->font, text, tw->color);
  if (!s) {
    fprintf(stderr, "TTF_RenderUTF8_Blended error: %s\n", TTF_GetError());
    tw->tex = NULL;
    return;
  }
  tw->tex = SDL_CreateTextureFromSurface(tw->renderer, s);
  tw->base.rect.w = s->w;
  tw->base.rect.h = s->h;
  SDL_FreeSurface(s);
}

void text_wrapper_destroy(UIWidget_t *w) {
  TextWrapperWidget_t *tw = (TextWrapperWidget_t *)w;
  if (tw->text)
    ui_widget_destroy(&tw->text->base);
  free(tw);
}
