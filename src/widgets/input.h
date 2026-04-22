#ifndef __INPUT_WIDGET_H
#define __INPUT_WIDGET_H

#include "../dc_config.h"
#include "ui_widget.h"

typedef struct {
  UIWidget_t base;

  char buf[MAX_INPUT_LENGTH];
  size_t len;
  size_t cursor;      /* byte offset of insertion point */
  int scroll_offset;  /* pixels of text hidden on the left */
  Uint32 blink_epoch; /* SDL_GetTicks() at last cursor/edit action */
  bool focused;
  bool masked; /* render as asterisks (e.g. for passwords) */
  ConfigType type;
  TTF_Font *font;
  long max_val;   /* for numeric types: upper bound (0 = no limit) */
  size_t max_len; /* for string types: max byte length (0 = MAX_INPUT_LENGTH-1) */
} InputWidget_t;

/* Create an input widget with an optional initial value, font, pixel width,
 * and ConfigType used for character validation. */
InputWidget_t *input_widget_create(const char *initial, TTF_Font *font, int w, ConfigType type);

/* Append text (e.g. from SDL_TEXTINPUT). Returns true if any text was added. */
bool input_widget_append(InputWidget_t *iw, const char *text);

/* Remove the UTF-8 character immediately before the cursor. */
void input_widget_backspace(InputWidget_t *iw);

/* Move cursor one UTF-8 character to the left or right. */
void input_widget_cursor_left(InputWidget_t *iw);
void input_widget_cursor_right(InputWidget_t *iw);

/* Jump cursor to the start or end of the text. */
void input_widget_cursor_home(InputWidget_t *iw);
void input_widget_cursor_end(InputWidget_t *iw);

/* Replace the buffer contents with the given text. */
void input_widget_set_text(InputWidget_t *iw, const char *text);

/* Get the current buffer contents. */
const char *input_widget_get_text(const InputWidget_t *iw);

#endif
