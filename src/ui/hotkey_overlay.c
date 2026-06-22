/*
 hotkey_overlay.c
 https://github.com/Dealer-s-Choice/dealers_choice

 Written by Claude (Anthropic, Opus 4.8) at Andy's direction.

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

#include "hotkey_overlay.h"

#include "globals_gui.h" /* g_center, g_viewport, FONT_*, DC_TEXT_*, draw_3d_border */
#include "hotkey_table.h"
#include "hotkeys.h"   /* hotkey_for_config_key */
#include "translate.h" /* _() */
#include "ui_widget.h"
#include "widgets/text.h"

#include <stdio.h>

/* Plenty of headroom over g_hotkey_def_count (currently 8); avoids a VLA. */
#define MAX_ACTION_ROWS 32

static int measure_w(TTF_Font *f, const char *s) {
  int w = 0;
  TTF_SizeUTF8(f, s, &w, NULL);
  return w;
}

/* Render one throwaway text line at (x, y).  The panel is only drawn while the
 * player has paused to read it, so the per-frame create/destroy cost is moot. */
static void draw_line(TTF_Font *f, const char *s, SDL_Color color, int x, int y) {
  TextWidget_t *t = text_widget_create(s, f, color);
  if (!t)
    return;
  ui_widget_place(&t->base, x, y);
  ui_widget_render(&t->base);
  ui_widget_destroy(&t->base);
}

static bool fixed_key_shown(size_t i, bool in_game) {
  return in_game || !g_fixed_keys[i].in_game_only;
}

void hotkey_overlay_render(SDL_Renderer *renderer, const Font_t *font, bool in_game) {
  TTF_Font *title_font = font->fonts[FONT_TITLE];
  TTF_Font *head_font = font->fonts[FONT_BOLD];
  TTF_Font *row_font = font->fonts[FONT_DEFAULT];

  /* Advance by each font's own height (the title is much taller than a row), so
   * lines never overlap vertically the way a single fixed row height did. */
  int title_h = 0, head_h = 0, row_h = 0;
  TTF_SizeUTF8(title_font, "Ag", NULL, &title_h);
  TTF_SizeUTF8(head_font, "Ag", NULL, &head_h);
  TTF_SizeUTF8(row_font, "Ag", NULL, &row_h);

  const int pad = 28;     /* inner padding around the content        */
  const int gap = 8;      /* vertical space after each line          */
  const int col_gap = 48; /* horizontal space between the columns    */

  // TRANSLATORS: Title of the F1 panel that lists keyboard shortcuts.
  const char *title = _("Keys");
  // TRANSLATORS: Section heading. "Settings" is the name of the Settings menu
  // button/screen; translate it the same way it is translated there.
  const char *act_head = _("Action keys (set in Settings)");
  // TRANSLATORS: Section heading for keys the player cannot change.
  const char *fixed_head = _("Fixed keys");

  /* Resolve the live key name for each configurable action (in-game only). */
  char action_key[MAX_ACTION_ROWS][48];
  size_t action_rows = g_hotkey_def_count < MAX_ACTION_ROWS ? g_hotkey_def_count : MAX_ACTION_ROWS;
  for (size_t i = 0; i < action_rows; i++) {
    SDL_Keycode kc = hotkey_for_config_key(g_hotkey_defs[i].config_key);
    const char *kn = (kc != SDLK_UNKNOWN) ? SDL_GetKeyName(kc) : NULL;
    snprintf(action_key[i], sizeof(action_key[i]), "%s",
             (kn && *kn) ? kn : g_hotkey_defs[i].default_key);
  }

  /* Size the panel and the key column from the actual rendered text widths, so a
   * long description can never run under the key column. */
  int max_desc = 0, max_key = 0, max_hdr = 0;
#define WIDEN(var, w)                                                                                \
  do {                                                                                              \
    int _w = (w);                                                                                  \
    if (_w > (var))                                                                                 \
      (var) = _w;                                                                                   \
  } while (0)

  WIDEN(max_hdr, measure_w(title_font, title));
  WIDEN(max_hdr, measure_w(head_font, fixed_head));
  if (in_game) {
    WIDEN(max_hdr, measure_w(head_font, act_head));
    for (size_t i = 0; i < action_rows; i++) {
      WIDEN(max_desc, measure_w(row_font, _(g_hotkey_defs[i].description)));
      WIDEN(max_key, measure_w(row_font, action_key[i]));
    }
  }
  size_t shown_fixed = 0;
  for (size_t i = 0; i < g_fixed_key_count; i++) {
    if (!fixed_key_shown(i, in_game))
      continue;
    shown_fixed++;
    WIDEN(max_desc, measure_w(row_font, _(g_fixed_keys[i].description)));
    WIDEN(max_key, measure_w(row_font, g_fixed_keys[i].keys));
  }
#undef WIDEN

  int rows_w = max_desc + col_gap + max_key;
  int content_w = rows_w > max_hdr ? rows_w : max_hdr;

  int panel_h = pad * 2;
  panel_h += title_h + gap;
  if (in_game) {
    panel_h += head_h + gap;
    panel_h += (int)action_rows * (row_h + gap);
  }
  panel_h += row_h + gap; /* blank spacer before the Fixed keys section */
  panel_h += head_h + gap;
  panel_h += (int)shown_fixed * (row_h + gap);

  int panel_w = pad * 2 + content_w;
  int panel_x = g_center.x - panel_w / 2;
  int panel_y = g_center.y - panel_h / 2;

  /* Dim the whole screen, then draw the opaque panel on top. */
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 160);
  SDL_RenderFillRect(renderer, &(SDL_Rect){0, 0, g_viewport.w + g_viewport.x * 2,
                                           g_viewport.h + g_viewport.y * 2});
  SDL_Rect panel = {panel_x, panel_y, panel_w, panel_h};
  SDL_SetRenderDrawColor(renderer, 25, 40, 30, 245);
  SDL_RenderFillRect(renderer, &panel);
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
  draw_3d_border(renderer, panel, 8);

  int text_x = panel_x + pad;
  int key_x = text_x + max_desc + col_gap;
  int y = panel_y + pad;

  draw_line(title_font, title, DC_TEXT_ON_DARK, text_x, y);
  y += title_h + gap;

  if (in_game) {
    draw_line(head_font, act_head, DC_TEXT_MUTED, text_x, y);
    y += head_h + gap;
    for (size_t i = 0; i < action_rows; i++) {
      draw_line(row_font, _(g_hotkey_defs[i].description), DC_TEXT_ON_DARK, text_x, y);
      draw_line(row_font, action_key[i], DC_TEXT_ON_DARK, key_x, y);
      y += row_h + gap;
    }
  }

  y += row_h + gap; /* spacer */

  draw_line(head_font, fixed_head, DC_TEXT_MUTED, text_x, y);
  y += head_h + gap;
  for (size_t i = 0; i < g_fixed_key_count; i++) {
    if (!fixed_key_shown(i, in_game))
      continue;
    draw_line(row_font, _(g_fixed_keys[i].description), DC_TEXT_ON_DARK, text_x, y);
    draw_line(row_font, g_fixed_keys[i].keys, DC_TEXT_ON_DARK, key_x, y);
    y += row_h + gap;
  }
}

bool hotkey_overlay_handle_event(const SDL_Event *e, bool *visible) {
  if (e->type != SDL_KEYDOWN)
    return false;
  if (e->key.keysym.sym == SDLK_F1) {
    *visible = !*visible;
    return true;
  }
  if (*visible) {
    if (e->key.keysym.sym == SDLK_ESCAPE)
      *visible = false;
    return true; /* swallow every other key while the panel is open */
  }
  return false;
}
