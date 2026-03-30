#ifndef __LINKS_H
#define __LINKS_H

#include <stddef.h>

#include "widgets/link.h"

typedef struct {
  const char *text;
  const char *url;
} LinkDef_t;
extern const LinkDef_t LINK_DEFS[];
extern const size_t LINK_DEFS_COUNT;

void layout_links(LinkWidget_t **links, size_t count);

#endif
