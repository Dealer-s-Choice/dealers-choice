#include "input.h"
#include "globals.h"
#include <stdlib.h>

#define CURSOR_BLINK_MS 500

/* Returns true if every character in `text` is acceptable for the given type. */
static bool chars_allowed(const char *text, ConfigType type, const char *current_buf) {
  for (const char *p = text; *p; p++) {
    switch (type) {
    case CFG_TYPE_INT:
      /* Allow a leading '-' only when the buffer is currently empty */
      if (*p == '-' && p == text && current_buf[0] == '\0')
        continue;
      if (*p < '0' || *p > '9')
        return false;
      break;
    case CFG_TYPE_UINT8:
    case CFG_TYPE_UINT16:
    case CFG_TYPE_UINT32:
      if (*p < '0' || *p > '9')
        return false;
      break;
    case CFG_TYPE_BOOL:
      /* Accept y/n/t/f/1/0 (case-insensitive first char) */
      {
        char c = (*p >= 'A' && *p <= 'Z') ? (*p + 32) : *p;
        if (c != 'y' && c != 'n' && c != 't' && c != 'f' && c != '1' && c != '0')
          return false;
      }
      break;
    case CFG_TYPE_STRING:
    default:
      /* Accept any printable ASCII (SDL text input already filters most junk) */
      if ((unsigned char)*p < 0x20)
        return false;
      break;
    }
  }
  return true;
}

static void input_widget_render(UIWidget_t *w) {
  InputWidget_t *iw = (InputWidget_t *)w;
  SDL_Renderer *r = g_sdl_context->renderer;

  /* Background */
  SDL_SetRenderDrawColor(r, 30, 30, 30, 255);
  SDL_RenderFillRect(r, &w->rect);

  /* Border: yellow when focused, white otherwise */
  SDL_Color border_color =
      iw->focused ? (SDL_Color){255, 220, 0, 255} : (SDL_Color){200, 200, 200, 255};
  SDL_SetRenderDrawColor(r, border_color.r, border_color.g, border_color.b, border_color.a);
  SDL_RenderDrawRect(r, &w->rect);
  /* Double border when focused */
  if (iw->focused) {
    SDL_Rect inner = {w->rect.x + 1, w->rect.y + 1, w->rect.w - 2, w->rect.h - 2};
    SDL_RenderDrawRect(r, &inner);
  }

  /* Text */
  int text_x = w->rect.x + 8;

  if (iw->len > 0 && iw->font) {
    SDL_Color white = {255, 255, 255, 255};
    SDL_Rect pos = {text_x, 0, 0, 0};
    int tw, th;
    if (TTF_SizeUTF8(iw->font, iw->buf, &tw, &th) == 0)
      pos.y = w->rect.y + (w->rect.h - th) / 2;
    else
      pos.y = w->rect.y + 4;

    SDL_Surface *surf = TTF_RenderUTF8_Blended(iw->font, iw->buf, white);
    if (surf) {
      SDL_Texture *tex = SDL_CreateTextureFromSurface(r, surf);
      if (tex) {
        SDL_Rect dst = {pos.x, pos.y, surf->w, surf->h};
        SDL_RenderCopy(r, tex, NULL, &dst);
        SDL_DestroyTexture(tex);
      }
      SDL_FreeSurface(surf);
    }
  }

  /* Blinking cursor when focused */
  if (iw->focused) {
    bool cursor_visible = (SDL_GetTicks() / CURSOR_BLINK_MS) % 2 == 0;
    if (cursor_visible) {
      int cursor_x = text_x;
      if (iw->len > 0 && iw->font) {
        int tw, th;
        if (TTF_SizeUTF8(iw->font, iw->buf, &tw, &th) == 0)
          cursor_x = text_x + tw + 1;
      }
      int cursor_top = w->rect.y + 4;
      int cursor_bot = w->rect.y + w->rect.h - 4;
      SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
      SDL_RenderDrawLine(r, cursor_x, cursor_top, cursor_x, cursor_bot);
    }
  }
}

static void input_widget_destroy(UIWidget_t *w) { free(w); }

InputWidget_t *input_widget_create(const char *initial, TTF_Font *font, int w, ConfigType type) {
  InputWidget_t *iw = calloc(1, sizeof(*iw));
  if (!iw)
    return NULL;

  iw->type = type;
  iw->font = font;

  if (initial && *initial) {
    size_t n = strlen(initial);
    if (n >= MAX_INPUT_LENGTH)
      n = MAX_INPUT_LENGTH - 1;
    memcpy(iw->buf, initial, n);
    iw->buf[n] = '\0';
    iw->len = n;
  }

  int th = 20;
  if (font)
    TTF_SizeUTF8(font, "Ag", NULL, &th);

  iw->base.rect.w = w;
  iw->base.rect.h = th + 16;
  iw->base.render = input_widget_render;
  iw->base.destroy = input_widget_destroy;

  return iw;
}

bool input_widget_append(InputWidget_t *iw, const char *text) {
  if (!iw || !text || !*text)
    return false;

  size_t add_len = strlen(text);
  if (iw->len + add_len >= MAX_INPUT_LENGTH)
    return false;

  if (!chars_allowed(text, iw->type, iw->buf))
    return false;

  memcpy(iw->buf + iw->len, text, add_len);
  iw->len += add_len;
  iw->buf[iw->len] = '\0';

  if (iw->max_val != 0) {
    long v = strtol(iw->buf, NULL, 10);
    if (v > iw->max_val) {
      iw->len -= add_len;
      iw->buf[iw->len] = '\0';
      return false;
    }
  }

  return true;
}

void input_widget_backspace(InputWidget_t *iw) {
  if (!iw || iw->len == 0)
    return;

  /* Walk back one UTF-8 character (continuation bytes start with 10xxxxxx) */
  do {
    iw->len--;
  } while (iw->len > 0 && ((unsigned char)iw->buf[iw->len] & 0xC0) == 0x80);

  iw->buf[iw->len] = '\0';
}

const char *input_widget_get_text(const InputWidget_t *iw) { return iw ? iw->buf : ""; }

void input_widget_set_text(InputWidget_t *iw, const char *text) {
  if (!iw)
    return;
  iw->len = 0;
  iw->buf[0] = '\0';
  if (text && *text)
    input_widget_append(iw, text);
}
