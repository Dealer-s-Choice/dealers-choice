#ifndef __BUTTON_WIDGET_H
#define __BUTTON_WIDGET_H

#include <SDL2/SDL.h>

#include "graphics.h"
#include "style.h"
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
  UIWidget_t base;
  TextWidget_t *text;
  TextWidget_t *text_disabled;
  Color_t color;
  SDL_Keycode hotkey;
  Clicked_t click;
  bool interactive;
} ButtonWidget_t;

ButtonWidget_t *button_widget_create_styled(const char *text, const ButtonRole_t *role,
                                            TTF_Font *const *fonts, SDL_Keycode hotkey);

#endif
