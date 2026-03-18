#ifndef __PING_H
#define __PING_H

#include "text.h"
#include "ui_widget.h"

typedef struct {
  UIWidget_t base;

  TextWidget_t *text;
  int ping;
} PingWidget_t;

PingWidget_t *ping_widget_create(int ping, TTF_Font *font, SDL_Color color);

void ping_widget_update(PingWidget_t *pw, int ping);

#endif
