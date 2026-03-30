#ifndef __LINK_WIDGET_H
#define __LINK_WIDGET_H

#include "text.h"
#include "ui_widget.h"

typedef struct {
  UIWidget_t base;
  TextWidget_t *text_normal;
  TextWidget_t *text_hovered;
  const char *url;
} LinkWidget_t;

LinkWidget_t *link_widget_create(const char *text, const char *url, TTF_Font *font);

#endif
