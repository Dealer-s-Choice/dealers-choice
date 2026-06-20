#ifndef __ROUND_BUTTON_WIDGET_H
#define __ROUND_BUTTON_WIDGET_H

#include "ui_widget.h"

/* A small round, shaded ("3D ball") button with no label. The whole bounding
 * box (base.rect) is the click target; base.hovered brightens it. Used for the
 * Connect cell in the server-list tables. */
typedef struct {
  UIWidget_t base; /* base.rect = bounding box (diameter x diameter) */
  SDL_Color color; /* base fill; shaded lighter toward the top-left */
} RoundButtonWidget_t;

RoundButtonWidget_t *round_button_create(int diameter, SDL_Color color);

#endif
