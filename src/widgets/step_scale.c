#include "step_scale.h"
#include "globals_gui.h"
#include "style.h"
#include <stdio.h>
#include <stdlib.h>

/* Desaturate a color toward its luma so an inactive scale reads as grayed. */
static SDL_Color wash(SDL_Color c) {
  Uint8 g = (Uint8)((c.r * 54 + c.g * 183 + c.b * 19) >> 8);
  return (SDL_Color){g, g, g, c.a};
}

static const SDL_Color STEP_TRACK_ORANGE = {235, 140, 30, 255};

/* 3D track bar: orange when active, washed gray otherwise. */
static void draw_track_bar(SDL_Renderer *r, SDL_Rect bar, bool active) {
  SDL_Color b = active ? STEP_TRACK_ORANGE : wash(STEP_TRACK_ORANGE);
  SDL_SetRenderDrawColor(r, b.r, b.g, b.b, 255);
  SDL_RenderFillRect(r, &bar);
  SDL_SetRenderDrawColor(r, (Uint8)(b.r < 215 ? b.r + 40 : 255),
                         (Uint8)(b.g < 200 ? b.g + 55 : 255), (Uint8)(b.b < 215 ? b.b + 40 : 255),
                         255);
  SDL_RenderDrawLine(r, bar.x, bar.y, bar.x + bar.w - 1, bar.y);
  SDL_SetRenderDrawColor(r, (Uint8)(b.r > 12 ? b.r - 12 : 0), (Uint8)(b.g > 35 ? b.g - 35 : 0),
                         (Uint8)(b.b > 18 ? b.b - 18 : 0), 255);
  SDL_RenderDrawLine(r, bar.x, bar.y + bar.h - 1, bar.x + bar.w - 1, bar.y + bar.h - 1);
}

static void step_scale_render(UIWidget_t *w) {
  StepScaleWidget_t *s = (StepScaleWidget_t *)w;
  SDL_Renderer *r = s->renderer;

  draw_track_bar(r, (SDL_Rect){w->rect.x, s->track_cy - 4, w->rect.w, 8}, s->active);

  for (int i = 0; i < s->count; i++) {
    bool sel = s->active && (i == s->selected);
    SDL_Color nc = !s->active      ? (SDL_Color){90, 90, 90, 255}
                   : sel           ? (SDL_Color){255, 190, 70, 255}
                   : s->enabled[i] ? (SDL_Color){255, 190, 70, 255}
                                   : (SDL_Color){60, 60, 60, 255};
    int nw = sel ? 6 : 4;
    int nh = sel ? w->rect.h / 4 : w->rect.h / 6;
    SDL_SetRenderDrawColor(r, nc.r, nc.g, nc.b, 255);
    SDL_RenderFillRect(r, &(SDL_Rect){s->notch_x[i] - nw / 2, s->track_cy - nh / 2, nw, nh});
  }

  if (s->label && s->active)
    ui_widget_render(&s->label->base);
  if (s->owed_label && s->active && s->owed > 0)
    ui_widget_render(&s->owed_label->base);
}

StepScaleWidget_t *step_scale_create(const uint32_t *values, const SDL_Keycode *hotkeys, int count,
                                     TTF_Font *label_font) {
  if (count < 1 || count > STEP_SCALE_MAX)
    return NULL;

  StepScaleWidget_t *s = calloc(1, sizeof(*s));
  if (!s)
    return NULL;

  s->renderer = g_sdl_context->renderer;
  s->count = count;
  s->selected = 0;
  s->active = true;
  for (int i = 0; i < count; i++) {
    s->values[i] = values[i];
    s->hotkeys[i] = hotkeys[i];
    s->enabled[i] = true;
  }

  s->label = text_widget_create("0", label_font, DC_TEXT_ON_DARK);
  if (!s->label) {
    free(s);
    return NULL;
  }

  s->owed_label = text_widget_create("", label_font, DC_TEXT_MUTED);
  if (!s->owed_label) {
    ui_widget_destroy(&s->label->base);
    free(s);
    return NULL;
  }
  s->owed = 0;

  s->base.render = step_scale_render;
  s->base.enabled = true;
  return s;
}

void step_scale_destroy(StepScaleWidget_t *s) {
  if (!s)
    return;
  if (s->label)
    ui_widget_destroy(&s->label->base);
  if (s->owed_label)
    ui_widget_destroy(&s->owed_label->base);
  free(s);
}

void step_scale_layout(StepScaleWidget_t *s, SDL_Rect region) {
  s->base.rect = region;
  s->track_cy = region.y + region.h * 2 / 3;

  int seg_w = region.w / s->count;
  int end_pad = seg_w / 4; /* gap before first / after last notch */
  int span = region.w - 2 * end_pad;
  for (int i = 0; i < s->count; i++) {
    s->notch_x[i] =
        (s->count == 1) ? region.x + region.w / 2 : region.x + end_pad + span * i / (s->count - 1);
    /* hit target = the whole segment, so a click on or near the notch registers */
    s->hit[i] = (SDL_Rect){region.x + i * seg_w, region.y, seg_w, region.h};
  }

  if (s->label) {
    int sel = (s->selected >= 0 && s->selected < s->count) ? s->selected : 0;
    char buf[16];
    snprintf(buf, sizeof buf, "%u", (unsigned)s->values[sel]);
    text_widget_set_text(s->label, buf);

    /* Center the value and the owed "(N)" together as one group, so the pair
     * stays centered in the region rather than the value being centered and the
     * owed amount hanging off to the right. */
    const bool show_owed = (s->owed_label && s->owed > 0);
    const int gap = s->label->base.rect.h / 2;
    int owed_w = 0;
    if (show_owed) {
      char obuf[20];
      snprintf(obuf, sizeof obuf, "(%u)", (unsigned)s->owed);
      text_widget_set_text(s->owed_label, obuf);
      owed_w = s->owed_label->base.rect.w;
    }
    const int group_w = s->label->base.rect.w + (show_owed ? gap + owed_w : 0);
    const int gx = region.x + (region.w - group_w) / 2;
    ui_widget_place(&s->label->base, gx, region.y);
    if (show_owed)
      ui_widget_place(&s->owed_label->base, gx + s->label->base.rect.w + gap, region.y);
  }
}

void step_scale_set_owed(StepScaleWidget_t *s, uint32_t owed) {
  s->owed = owed;
}

bool step_scale_handle(StepScaleWidget_t *s, const SDL_Event *e, SDL_Point mouse) {
  if (!s->active)
    return false;

  bool changed = false;

  /* If the current selection became disabled, advance to the next enabled notch. */
  if (s->selected >= 0 && s->selected < s->count && !s->enabled[s->selected]) {
    for (int k = 1; k <= s->count; k++) {
      int j = (s->selected + k) % s->count;
      if (s->enabled[j]) {
        s->selected = j;
        changed = true;
        break;
      }
    }
  }

  for (int i = 0; i < s->count; i++) {
    if (!s->enabled[i])
      continue;
    bool click = (e->type == SDL_MOUSEBUTTONDOWN && SDL_PointInRect(&mouse, &s->hit[i]));
    bool key = (e->type == SDL_KEYDOWN && e->key.keysym.sym == s->hotkeys[i]);
    if (click || key) {
      if (s->selected != i) {
        s->selected = i;
        changed = true;
      }
      break;
    }
  }
  return changed;
}

uint32_t step_scale_value(const StepScaleWidget_t *s) {
  int sel = (s->selected >= 0 && s->selected < s->count) ? s->selected : 0;
  return s->values[sel];
}
