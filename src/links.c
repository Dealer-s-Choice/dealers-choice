#include "config.h"
#include "globals.h"
#include "links.h"
#include "translate.h"
#include "util.h"

const LinkDef_t LINK_DEFS[] = {
    /* TRANSLATORS: "Discord", "Lazarus Project" should not be translated */
    {N_("Discord Channel (on Lazarus Project Server)"),
     "https://discord.com/channels/1295630985429516299/1385298664192217138"},
    {"Matrix", "https://matrix.to/#/#dealers-choice:matrix.org"},
    {N_("Website"), DEALERSCHOICE_URL}};

const size_t LINK_DEFS_COUNT = ARRAY_SIZE(LINK_DEFS);

void layout_links(LinkWidget_t **links, size_t count) {
  int center_x = g_center.x + 200;

  for (size_t i = 0; i < count; i++) {
    links[i]->base.rect.x = center_x - (links[i]->base.rect.w / 2);
    links[i]->base.rect.y = (g_viewport.h - (links[i]->base.rect.h * 2)) -
                            (i * links[i]->base.rect.h) -
                            (i * (links[i]->base.rect.h * 0.4));
  }
}
