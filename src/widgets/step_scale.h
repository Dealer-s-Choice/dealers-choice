/*
 step_scale.h
 https://github.com/Dealer-s-Choice/dealers_choice

 MIT License

 Copyright (c) 2026 Andy Alt

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.

*/

#ifndef __STEP_SCALE_H
#define __STEP_SCALE_H

#include "graphics.h"
#include "text.h"
#include "ui_widget.h"
#include <stdint.h>

#define STEP_SCALE_MAX 8

/* A notched horizontal scale: the user clicks on (or near) one of N distinct
 * notches to select a value. It does not slide. The currently selected value is
 * shown as a label above the track. */
typedef struct {
  UIWidget_t base; /* base.rect = the scale region (set by step_scale_layout) */
  SDL_Renderer *renderer;
  TextWidget_t *label;       /* selected value, rendered above the track */
  TextWidget_t *owed_label;  /* "(N)" amount owed, drawn right of the value (#60) */
  uint32_t owed;             /* amount to call; 0 hides owed_label */
  uint32_t values[STEP_SCALE_MAX];
  SDL_Keycode hotkeys[STEP_SCALE_MAX];
  bool enabled[STEP_SCALE_MAX]; /* which notches are selectable (caller-managed) */
  SDL_Rect hit[STEP_SCALE_MAX]; /* per-notch clickable segment (set by layout) */
  int notch_x[STEP_SCALE_MAX];  /* notch tick centre x (set by layout) */
  int count;
  int selected;
  int track_cy; /* track centre-line y (set by layout) */
  bool active;  /* false washes the colors out (not the player's money turn) */
} StepScaleWidget_t;

StepScaleWidget_t *step_scale_create(const uint32_t *values, const SDL_Keycode *hotkeys, int count,
                                     TTF_Font *label_font);
void step_scale_destroy(StepScaleWidget_t *s);

/* Lay the notches / hit-segments out within `region` and position the value
 * label above the track. Call once per frame before rendering. */
void step_scale_layout(StepScaleWidget_t *s, SDL_Rect region);

/* Process one polled event: auto-advances off a disabled notch, then selects on
 * a click within (or near) a notch or on its hotkey. Returns true if the
 * selection changed. */
bool step_scale_handle(StepScaleWidget_t *s, const SDL_Event *e, SDL_Point mouse);

uint32_t step_scale_value(const StepScaleWidget_t *s);

/* Set the amount owed to call, shown in parens right of the selected value.
 * 0 hides it. Call before step_scale_layout. */
void step_scale_set_owed(StepScaleWidget_t *s, uint32_t owed);

#endif
