#include "links.h"
#include "config.h"
#include "globals_gui.h"
#include "translate.h"
#include "util.h"

const LinkDef_t LINK_DEFS[] = {
    /* TRANSLATORS: "Discord", "Lazarus Project" should not be translated */
    {N_("Discord Channel (on Lazarus Project Server)"),
     "https://discord.com/channels/1295630985429516299/1385298664192217138"},
    {N_("Website"), DEALERSCHOICE_URL}};

/* LINK_DEFS_COUNT is in links.h because main.c sizes an array with it, so it
 * cannot be derived from this array. Editing one without the other would walk
 * off the end of it. */
_Static_assert(sizeof LINK_DEFS / sizeof LINK_DEFS[0] == LINK_DEFS_COUNT,
               "LINK_DEFS_COUNT is out of sync with LINK_DEFS");

void layout_links(LinkWidget_t **links, size_t count) {
  int center_x = g_layout.menu.links_center_x;

  for (size_t i = 0; i < count; i++) {
    links[i]->base.rect.x = center_x - (links[i]->base.rect.w / 2);
    links[i]->base.rect.y = (g_viewport.h - (links[i]->base.rect.h * 2)) -
                            (i * links[i]->base.rect.h) - (i * (links[i]->base.rect.h * 2 / 5));
  }
}
