#ifndef __NICK_H
#define __NICK_H

#include "text.h"
#include "ui_widget.h"

typedef struct {
  UIWidget_t base;

  TextWidget_t *text;
  int id;
  bool highlight;
} NickWidget_t;

NickWidget_t *nick_widget_create(const char *nick, const int8_t id, TTF_Font *font);

#endif
