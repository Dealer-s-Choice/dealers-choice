/*
 style.c
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

#include <canfigger.h>
#include <stdio.h>
#include <string.h>

#include "graphics.h"
#include "style.h"
#include "util.h"

StyleConfig_t g_style_cfg;

static const struct {
  const char *name;
  SDL_Color color;
} color_lookup[] = {
    {"white",       {255, 255, 255, 255}},
    {"lightgray",   {200, 200, 200, 255}},
    {"gray",        {128, 128, 128, 255}},
    {"darkgray",    { 64,  64,  64, 255}},
    {"black",       {  0,   0,   0, 255}},
    {"red",         {255,   0,   0, 255}},
    {"green",       {  0, 255,   0, 255}},
    {"table_green", {  0, 125,   0, 255}},
    {"blue",        {  0,   0, 255, 255}},
    {"yellow",      {255, 255,   0, 255}},
    {"cyan",        {  0, 255, 255, 255}},
    {"magenta",     {255,   0, 255, 255}},
    {"orange",      {255, 165,   0, 255}},
    {"gold",        {240, 204,  48, 255}},
    {"purple",      {128,   0, 128, 255}},
    {"brown",       {165,  42,  42, 255}},
    {"pink",        {255, 192, 203, 255}},
    {"teal",        {  0, 128, 128, 255}},
};

static const struct {
  const char *name;
  int idx;
} font_lookup[] = {
    {"card",         FONT_CARD},
    {"default",      FONT_DEFAULT},
    {"default_bold", FONT_DEFAULT_BOLD},
    {"bold",         FONT_BOLD},
    {"link",         FONT_LINK},
    {"status_msg",   FONT_STATUS_MSG},
    {"title",        FONT_TITLE},
    {"version",      FONT_VERSION},
    {"wild_select",  FONT_WILD_SELECT},
};

static SDL_Color parse_color(const char *s) {
  if (!s || !*s)
    return (SDL_Color){0, 0, 0, 255};
  if (s[0] == '#') {
    unsigned r, g, b, a = 255;
    if (sscanf(s + 1, "%02x%02x%02x%02x", &r, &g, &b, &a) >= 3)
      return (SDL_Color){(Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a};
  }
  for (size_t i = 0; i < ARRAY_SIZE(color_lookup); i++)
    if (strcasecmp(s, color_lookup[i].name) == 0)
      return color_lookup[i].color;
  SDL_Log("style: unknown color '%s'", s);
  return (SDL_Color){0, 0, 0, 255};
}

static int parse_font(const char *s) {
  if (!s || !*s)
    return FONT_BOLD;
  for (size_t i = 0; i < ARRAY_SIZE(font_lookup); i++)
    if (strcasecmp(s, font_lookup[i].name) == 0)
      return font_lookup[i].idx;
  SDL_Log("style: unknown font '%s'", s);
  return FONT_BOLD;
}

typedef enum { STYLE_COLOR, STYLE_PAIR, STYLE_FONT } FieldType_t;

// clang-format off
static const struct {
  const char  *key;
  FieldType_t  type;
  size_t       offset;
} style_fields[] = {
  {"button_primary",  STYLE_PAIR,  offsetof(StyleConfig_t, button_primary)},
  {"button_danger",   STYLE_PAIR,  offsetof(StyleConfig_t, button_danger)},
  {"button_warn",     STYLE_PAIR,  offsetof(StyleConfig_t, button_warn)},
  {"button_cancel",   STYLE_PAIR,  offsetof(StyleConfig_t, button_cancel)},
  {"indicator_wild",  STYLE_PAIR,  offsetof(StyleConfig_t, indicator_wild)},
  {"indicator_game",  STYLE_PAIR,  offsetof(StyleConfig_t, indicator_game)},
  {"text_on_dark",    STYLE_COLOR, offsetof(StyleConfig_t, text_on_dark)},
  {"text_on_light",   STYLE_COLOR, offsetof(StyleConfig_t, text_on_light)},
  {"text_muted",      STYLE_COLOR, offsetof(StyleConfig_t, text_muted)},
  {"link_normal",     STYLE_COLOR, offsetof(StyleConfig_t, link_normal)},
  {"link_hover",      STYLE_COLOR, offsetof(StyleConfig_t, link_hover)},
  {"timer_bg",        STYLE_COLOR, offsetof(StyleConfig_t, timer_bg)},
  {"timer_elapsed",   STYLE_COLOR, offsetof(StyleConfig_t, timer_elapsed)},
  {"button_font",     STYLE_FONT,  offsetof(StyleConfig_t, button_font)},
  {"link_font",       STYLE_FONT,  offsetof(StyleConfig_t, link_font)},
};
// clang-format on

StyleConfig_t get_style_config(const char *data_dir) {
  StyleConfig_t cfg = {0};

  char *cfg_path = canfigger_path_join(data_dir, "layout.conf");
  if (!cfg_path)
    return cfg;

  struct Canfigger *node = canfigger_parse_file(cfg_path, ',');
  free(cfg_path);
  if (!node)
    return cfg;

  while (node) {
    const char *k = node->key;
    const char *v = node->value;

    for (size_t i = 0; i < ARRAY_SIZE(style_fields); i++) {
      if (strcasecmp(k, style_fields[i].key) != 0)
        continue;
      char *base = (char *)&cfg + style_fields[i].offset;
      if (style_fields[i].type == STYLE_FONT) {
        if (v) *(int *)base = parse_font(v);
      } else if (style_fields[i].type == STYLE_COLOR) {
        if (v) *(SDL_Color *)base = parse_color(v);
      } else {
        struct { SDL_Color bg, fg; } *pair = (void *)base;
        if (v) pair->bg = parse_color(v);
        char *attr = NULL;
        canfigger_free_current_attr_str_advance(node->attributes, &attr);
        if (attr) pair->fg = parse_color(attr);
      }
      break;
    }

    canfigger_free_current_key_node_advance(&node);
  }

  return cfg;
}
