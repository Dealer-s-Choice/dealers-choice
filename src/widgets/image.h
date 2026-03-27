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

/* Wrap an existing SDL_Texture without taking ownership. The texture is
 * NOT destroyed when the widget is destroyed. */
ImageWidget_t *image_widget_from_texture(SDL_Texture *tex, int w, int h);

#endif
