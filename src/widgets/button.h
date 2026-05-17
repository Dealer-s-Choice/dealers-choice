#ifndef __BUTTON_WIDGET_H
#define __BUTTON_WIDGET_H

#include <SDL2/SDL.h>

#include "graphics.h"
#include "text.h"

#define CLICKED_DEFAULT                                                                            \
  {                                                                                                \
      .start_time = 0,                                                                             \
      .duration = 80,                                                                              \
  }

typedef struct {
  uint32_t start_time;
  uint32_t duration; // in milliseconds
} Clicked_t;

typedef struct {
  SDL_Color bg;
  SDL_Color fg;
} Color_t;

typedef struct {
  EColorName_t bg;
  EColorName_t fg;
} EColor_t;

typedef struct {
  UIWidget_t base;
  TextWidget_t *text;
  TextWidget_t *text_disabled;
  Color_t color;
  SDL_Keycode hotkey;
  Clicked_t click;
  bool interactive;
} ButtonWidget_t;

ButtonWidget_t *button_widget_create(const char *text, EColor_t color, TTF_Font *font,
                                     SDL_Keycode hotkey);
ButtonWidget_t *button_widget_create_colored(const char *text, SDL_Color bg, SDL_Color fg,
                                             TTF_Font *font, SDL_Keycode hotkey);

#endif
