#ifndef __CHECKBOX_WIDGET_H
#define __CHECKBOX_WIDGET_H

#include "ui_widget.h"
#include <stdbool.h>

typedef struct {
  UIWidget_t base;

  SDL_Renderer *renderer;
  bool checked;
} CheckboxWidget_t;

/* Create a checkbox of the given pixel size with an initial checked state. */
CheckboxWidget_t *checkbox_widget_create(bool checked, int size);

#endif
