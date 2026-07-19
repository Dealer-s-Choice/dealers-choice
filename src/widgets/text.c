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

/* (Re)build tw->tex from `text` using tw's stored font/color/renderer, and set
 * the base rect to the rendered size. Any prior texture must already be freed by
 * the caller (this only overwrites the pointer). An empty string or a render
 * failure leaves tex NULL and the rect zeroed — text_widget_render no-ops on a
 * NULL texture. Shared by text_widget_create and text_widget_set_text. */
static void text_widget_rebuild(TextWidget_t *tw, const char *text) {
  tw->tex = NULL;
  tw->base.rect.w = 0;
  tw->base.rect.h = 0;

  if (text[0] == '\0')
    return;

  SDL_Surface *s = TTF_RenderUTF8_Blended(tw->font, text, tw->color);
  if (!s) {
    dc_log(DC_LOG_ERROR, "TTF_RenderUTF8_Blended error: %s", TTF_GetError());
    return;
  }
  tw->tex = SDL_CreateTextureFromSurface(tw->renderer, s);
  tw->base.rect.w = s->w;
  tw->base.rect.h = s->h;
  SDL_FreeSurface(s);
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
    dc_log(DC_LOG_WARN, "text_widget_create: text too long, refusing render");
    return NULL;
  }

  TextWidget_t *tw = calloc(1, sizeof(*tw));
  if (!tw)
    return NULL;

  tw->renderer = renderer;
  tw->font = font;
  tw->color = color;
  tw->text = dc_strdup(text);

  /* An empty string / render failure leaves tex NULL and a zero-size rect —
   * text_widget_render no-ops on a NULL texture rather than log-spamming. */
  text_widget_rebuild(tw, text);

  tw->base.render = text_widget_render;
  tw->base.destroy = text_widget_destroy;

  return tw;
}

void text_widget_set_text(TextWidget_t *tw, const char *text) {
  if (!tw || !text)
    return;

  if (text_too_long(text)) {
    dc_log(DC_LOG_WARN, "text_widget_set_text: text too long, refusing render");
    return;
  }

  if (tw->tex)
    SDL_DestroyTexture(tw->tex);

  free(tw->text);
  tw->text = dc_strdup(text);

  text_widget_rebuild(tw, text);
}

void text_wrapper_destroy(UIWidget_t *w) {
  TextWrapperWidget_t *tw = (TextWrapperWidget_t *)w;
  if (tw->text)
    ui_widget_destroy(&tw->text->base);
  free(tw);
}
