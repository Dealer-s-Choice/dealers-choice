#include "input.h"
#include "../graphics.h"
#include "globals_gui.h"
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
  draw_rect_border(r, w->rect);

  /* Text */
  int text_x = w->rect.x + g_layout_cfg.input_text_pad_x;
  int available_w = w->rect.w - g_layout_cfg.input_text_pad_x * 2;

  /* Build display string: asterisks for masked fields, raw buf otherwise */
  char masked_buf[MAX_INPUT_LENGTH];
  const char *display = iw->buf;
  if (iw->masked && iw->len > 0) {
    memset(masked_buf, '*', iw->len);
    masked_buf[iw->len] = '\0';
    display = masked_buf;
  }

  /* Measure pixel width of text up to the cursor */
  int cursor_pixel_w = 0, th = 20;
  if (iw->font) {
    char pre[MAX_INPUT_LENGTH];
    memcpy(pre, display, iw->cursor);
    pre[iw->cursor] = '\0';
    TTF_SizeUTF8(iw->font, pre, &cursor_pixel_w, &th);
  }

  /* Lazily adjust scroll_offset just enough to keep the cursor visible.
   * Only moves when the cursor leaves the visible window — no snapping. */
  if (cursor_pixel_w < iw->scroll_offset)
    iw->scroll_offset = cursor_pixel_w;
  else if (cursor_pixel_w > iw->scroll_offset + available_w)
    iw->scroll_offset = cursor_pixel_w - available_w;
  if (iw->scroll_offset < 0)
    iw->scroll_offset = 0;

  int draw_x = text_x - iw->scroll_offset;

  if (iw->len > 0 && iw->font) {
    int tw, draw_y = w->rect.y + (w->rect.h - th) / 2;
    TTF_SizeUTF8(iw->font, display, &tw, NULL);

    SDL_Surface *surf = TTF_RenderUTF8_Blended(iw->font, display, (SDL_Color){255, 255, 255, 255});
    if (surf) {
      SDL_Texture *tex = SDL_CreateTextureFromSurface(r, surf);
      if (tex) {
        SDL_Rect clip = {w->rect.x + 1, w->rect.y + 1, w->rect.w - 2, w->rect.h - 2};
        SDL_RenderSetClipRect(r, &clip);
        SDL_Rect dst = {draw_x, draw_y, surf->w, surf->h};
        SDL_RenderCopy(r, tex, NULL, &dst);
        SDL_RenderSetClipRect(r, NULL);
        SDL_DestroyTexture(tex);
      }
      SDL_FreeSurface(surf);
    }
  }

  /* Blinking cursor when focused */
  if (iw->focused) {
    bool cursor_visible = ((SDL_GetTicks() - iw->blink_epoch) / CURSOR_BLINK_MS) % 2 == 0;
    if (cursor_visible) {
      int cursor_x = text_x - iw->scroll_offset + cursor_pixel_w;
      SDL_SetRenderDrawColor(r, 255, 220, 0, 255); /* yellow: visible against white text */
      SDL_Rect caret = {cursor_x, w->rect.y + 4, 2, w->rect.h - 8};
      SDL_RenderFillRect(r, &caret);
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

  iw->cursor = iw->len;

  int th = 20;
  if (font)
    TTF_SizeUTF8(font, "Ag", NULL, &th);

  iw->base.rect.w = w;
  iw->base.rect.h = th + g_layout_cfg.input_h_pad;
  iw->base.render = input_widget_render;
  iw->base.destroy = input_widget_destroy;

  return iw;
}

bool input_widget_append(InputWidget_t *iw, const char *text) {
  if (!iw || !text || !*text)
    return false;

  size_t add_len = strlen(text);
  size_t limit = (iw->max_len > 0) ? iw->max_len : (MAX_INPUT_LENGTH - 1);
  if (iw->len + add_len > limit)
    return false;

  if (!chars_allowed(text, iw->type, iw->buf))
    return false;

  /* Make room for the new text at the cursor position */
  memmove(iw->buf + iw->cursor + add_len, iw->buf + iw->cursor, iw->len - iw->cursor + 1);
  memcpy(iw->buf + iw->cursor, text, add_len);
  iw->len += add_len;
  iw->cursor += add_len;
  iw->blink_epoch = SDL_GetTicks();

  if (iw->max_val != 0) {
    long v = strtol(iw->buf, NULL, 10);
    if (v > iw->max_val) {
      memmove(iw->buf + iw->cursor - add_len, iw->buf + iw->cursor, iw->len - iw->cursor + 1);
      iw->len -= add_len;
      iw->cursor -= add_len;
      return false;
    }
  }

  return true;
}

void input_widget_backspace(InputWidget_t *iw) {
  if (!iw || iw->cursor == 0)
    return;

  /* Walk back one UTF-8 character (continuation bytes start with 10xxxxxx) */
  size_t new_cursor = iw->cursor;
  do {
    new_cursor--;
  } while (new_cursor > 0 && ((unsigned char)iw->buf[new_cursor] & 0xC0) == 0x80);

  size_t char_len = iw->cursor - new_cursor;
  memmove(iw->buf + new_cursor, iw->buf + iw->cursor, iw->len - iw->cursor + 1);
  iw->len -= char_len;
  iw->cursor = new_cursor;
  iw->blink_epoch = SDL_GetTicks();
}

void input_widget_cursor_left(InputWidget_t *iw) {
  if (!iw || iw->cursor == 0)
    return;
  do {
    iw->cursor--;
  } while (iw->cursor > 0 && ((unsigned char)iw->buf[iw->cursor] & 0xC0) == 0x80);
  iw->blink_epoch = SDL_GetTicks();
}

void input_widget_cursor_right(InputWidget_t *iw) {
  if (!iw || iw->cursor >= iw->len)
    return;
  do {
    iw->cursor++;
  } while (iw->cursor < iw->len && ((unsigned char)iw->buf[iw->cursor] & 0xC0) == 0x80);
  iw->blink_epoch = SDL_GetTicks();
}

void input_widget_cursor_home(InputWidget_t *iw) {
  if (!iw)
    return;
  iw->cursor = 0;
  iw->blink_epoch = SDL_GetTicks();
}

void input_widget_cursor_end(InputWidget_t *iw) {
  if (!iw)
    return;
  iw->cursor = iw->len;
  iw->blink_epoch = SDL_GetTicks();
}

const char *input_widget_get_text(const InputWidget_t *iw) { return iw ? iw->buf : ""; }

void input_widget_set_text(InputWidget_t *iw, const char *text) {
  if (!iw)
    return;
  iw->len = 0;
  iw->cursor = 0;
  iw->scroll_offset = 0;
  iw->buf[0] = '\0';
  if (text && *text)
    input_widget_append(iw, text);
}
