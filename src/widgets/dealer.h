#ifndef __DEALER_H
#define __DEALER_H

#include "ui_widget.h"
#include <stdbool.h>

#define DEALER_WIDGET_SIZE 20

typedef struct {
  UIWidget_t base;
  bool is_dealer;
} DealerWidget_t;

DealerWidget_t *dealer_widget_create(bool is_dealer);

void dealer_widget_set(DealerWidget_t *dw, bool is_dealer);

#endif
