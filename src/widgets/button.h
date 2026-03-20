#ifndef __BUTTON_WIDGET_H
#define __BUTTON_WIDGET_H

#include "../button.h"
#include "text.h"

typedef struct {
  UIWidget_t base;
  TextWidget_t *text;
  TextWidget_t *text_disabled;
  Color_t color;
  SDL_Keycode hotkey;
  Clicked_t click;
  bool interactive;
  bool selected;
} ButtonWidget_t;

ButtonWidget_t *button_widget_create(const char *text, EColor_t color, TTF_Font *font,
                                     SDL_Keycode hotkey);

#endif
