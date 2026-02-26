#ifndef __LINKS_H
#define __LINKS_H

#include <stdbool.h>

#include "globals.h"

typedef struct {
  const char *text;
  const char *url;
  TTF_Font *font;
  // SDL_Color textColor;
  SDL_Renderer *renderer;
  // SDL_Color bg_color;
  // SDL_Color fg_color;
  SDL_Rect rect;
  bool hovered;
} Link_t;

typedef struct {
  const char *text;
  const char *url;
} LinkDef_t;
extern const LinkDef_t LINK_DEFS[];
extern const size_t LINK_DEFS_COUNT;

void init_links(Link_t *links, TTF_Font *font);

void render_link(Link_t *link);

void layout_links(Link_t *link, size_t count);

#endif
