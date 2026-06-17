#ifndef __LINKS_H
#define __LINKS_H

#include <stddef.h>

#include "widgets/link.h"

typedef struct {
  const char *text;
  const char *url;
} LinkDef_t;
extern const LinkDef_t LINK_DEFS[];
#define LINK_DEFS_COUNT 3

void layout_links(LinkWidget_t **links, size_t count);

#endif
