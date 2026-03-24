#ifndef __IMAGE_WIDGET_H
#define __IMAGE_WIDGET_H

#include "graphics.h"
#include "ui_widget.h"

typedef struct {
  UIWidget_t base;

  SDL_Renderer *renderer;
  SDL_Texture *tex;
} ImageWidget_t;

/* Load an image from path. If w and h are both > 0 the widget is scaled to
 * that size; otherwise the image's natural dimensions are used. */
ImageWidget_t *image_widget_create(const char *path, int w, int h);

#endif
