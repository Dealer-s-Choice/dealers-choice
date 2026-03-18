#ifndef __TEXT_H
#define __TEXT_H

#include "graphics.h"
#include "ui_widget.h"

typedef struct {
  UIWidget_t base;

  SDL_Renderer *renderer;

  SDL_Texture *tex;

  SDL_Color color;
  TTF_Font *font;

  char *text; // optional: store string for updates
} TextWidget_t;

typedef struct {
  UIWidget_t base;
  TextWidget_t *text;
} TextWrapperWidget_t;

TextWidget_t *text_widget_create(const char *text, TTF_Font *font, SDL_Color color);

void text_widget_set_text(TextWidget_t *tw, const char *text);

void text_wrapper_destroy(UIWidget_t *w);

#endif
