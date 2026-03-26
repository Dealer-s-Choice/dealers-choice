#ifndef __INPUT_WIDGET_H
#define __INPUT_WIDGET_H

#include "../dc_config.h"
#include "ui_widget.h"

typedef struct {
  UIWidget_t base;

  char buf[MAX_INPUT_LENGTH];
  size_t len;
  bool focused;
  ConfigType type;
  TTF_Font *font;
  long max_val; /* for numeric types: upper bound (0 = no limit) */
} InputWidget_t;

/* Create an input widget with an optional initial value, font, pixel width,
 * and ConfigType used for character validation. */
InputWidget_t *input_widget_create(const char *initial, TTF_Font *font, int w, ConfigType type);

/* Append text (e.g. from SDL_TEXTINPUT). Returns true if any text was added. */
bool input_widget_append(InputWidget_t *iw, const char *text);

/* Remove the last UTF-8 character. */
void input_widget_backspace(InputWidget_t *iw);

/* Replace the buffer contents with the given text. */
void input_widget_set_text(InputWidget_t *iw, const char *text);

/* Get the current buffer contents. */
const char *input_widget_get_text(const InputWidget_t *iw);

#endif
