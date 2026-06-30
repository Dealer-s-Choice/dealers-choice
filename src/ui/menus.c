/*
 menus.c
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

/* The pre-game menu screens: connect (with LAN discovery list), hotkeys, and
 * settings. Split out of main.c; main() drives connect/settings via menus.h. */

#include <canfigger.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "client.h"
#include "dc_config.h"
#include "game.h"
#include "globals_gui.h"
#include "graphics.h"
#include "hotkey_overlay.h"
#include "hotkey_table.h"
#include "hotkeys.h"
#include "lan_discovery.h"
#include "links.h"
#include "menus.h"
#include "net/registry.h"
#include "ping_probe.h"
#include "registry_fetch_thread.h"
#include "util.h"
#include "widgets/button.h"
#include "widgets/card.h"
#include "widgets/card_text_atlas.h"
#include "widgets/checkbox.h"
#include "widgets/image.h"
#include "widgets/input.h"
#include "widgets/diamond_button.h"
#include "widgets/text.h"

enum { LAN_MAX_SHOWN = 6 };

/* FNV-1a over the displayed fields, so the row buttons are rebuilt only when
 * the visible list actually changes. */
static uint64_t lan_list_sig(const LanGameInfo_t *list, int count) {
  uint64_t h = 1469598103934665603ULL;
  char buf[96];
  for (int i = 0; i < count; i++) {
    int m = snprintf(buf, sizeof(buf), "%s:%u:%u:%u:%d:%d|", list[i].ip, (unsigned)list[i].tcp_port,
                     (unsigned)list[i].player_count, (unsigned)list[i].max_players,
                     list[i].password_protected, list[i].in_progress);
    for (int j = 0; j < m && j < (int)sizeof(buf); j++) {
      h ^= (unsigned char)buf[j];
      h *= 1099511628211ULL;
    }
  }
  return h;
}

/* How connectable a discovered address is, for picking which one to show when a
 * server answers on several. Higher is better. */
static int addr_score(const char *ip) {
  if (strncmp(ip, "127.", 4) == 0 || strcmp(ip, "::1") == 0)
    return 0; /* loopback: same host only */
  if (strncmp(ip, "fe80", 4) == 0 || strncmp(ip, "FE80", 4) == 0)
    return 1; /* IPv6 link-local: only on that link */
  return 2;   /* routable LAN IPv4, or global/ULA IPv6 */
}

/* Insert g, or refresh the entry for the same server. Servers are matched by
 * instance_id, not ip:port: one server answers a discovery query on every
 * interface it's reachable on (IPv4 + several IPv6 link-local + loopback), so
 * keying on address would list it many times. The dynamic fields are always
 * refreshed from g; the displayed address is kept as the most-connectable of the
 * ones seen (so e.g. 192.168.x wins over fe80::%n and 127.0.0.1). Returns the
 * new count (unchanged if the list is full). */
static int lan_upsert(LanGameInfo_t *list, uint32_t *seen, int count, int max,
                      const LanGameInfo_t *g, uint32_t now) {
  for (int i = 0; i < count; i++) {
    if (list[i].instance_id == g->instance_id) {
      char prev_ip[sizeof list[i].ip];
      snprintf(prev_ip, sizeof prev_ip, "%s", list[i].ip);
      bool take_new_ip = addr_score(g->ip) > addr_score(prev_ip);
      list[i] = *g; /* refresh player_count / flags / name */
      if (!take_new_ip)
        snprintf(list[i].ip, sizeof list[i].ip, "%s", prev_ip);
      seen[i] = now;
      return count;
    }
  }
  if (count < max) {
    list[count] = *g;
    seen[count] = now;
    return count + 1;
  }
  return count;
}

/* Drop entries not seen within ttl ms; returns the compacted count. */
static int lan_expire(LanGameInfo_t *list, uint32_t *seen, int count, uint32_t now, uint32_t ttl) {
  int w = 0;
  for (int i = 0; i < count; i++) {
    if (now - seen[i] <= ttl) {
      if (w != i) {
        list[w] = list[i];
        seen[w] = seen[i];
      }
      w++;
    }
  }
  return w;
}

enum { REG_MAX_SHOWN = 8 };

/* A single server as shown in a list table. LanGameInfo_t and RegistryServer_t
 * have identical fields, so both LAN and registry rows map onto this and share
 * one table builder. */
typedef struct {
  const char *name;
  const char *ip;
  uint16_t port;
  uint8_t player_count;
  uint8_t max_players;
  bool in_progress;
  /* Server process start, unix seconds (0 = unknown). Registry rows carry the
   * value the server reported; LAN discovery has no such field, so LAN rows
   * leave it 0 and the Uptime cell shows "-". */
  uint64_t start_time;
  /* Connect-RTT latency in ms, or a PING_* sentinel (PING_PENDING while a
   * measurement is in flight, PING_UNREACHABLE if the probe failed). Filled in
   * from the background prober at table-build time. */
  int ping_ms;
} ServerRow_t;

enum { SERVER_TABLE_COLS = 7 }; /* Name | IP | Port | Players | Uptime | Ping | Connect */
/* Connect-cell diamond gem: its bounding box defines the column width and the
 * row height (it is the tallest widget in the row), so the diamond effectively
 * fills its cell — left/right points near the side margins, top point near the
 * top, bottom point at the full cell bottom (#93). Wider than tall reads as a
 * card-suit diamond sitting in a button slot. */
enum { CONNECT_BTN_W = 80, CONNECT_BTN_H = 30 };

/* Total laid-out width of a table (sum of column widths plus inter-column gaps). */
static int server_table_width(const UITable_t *t) {
  int w = 0;
  for (int c = 0; c < t->cols; c++)
    w += t->col_width[c];
  if (t->cols > 1)
    w += t->col_spacing * (t->cols - 1);
  return w;
}

/* Y just past the last row of a laid-out table (its visual bottom edge). */
static int server_table_bottom(const UITable_t *t) {
  int y = t->y;
  for (int r = 0; r < t->rows; r++)
    y += t->row_height[r] + t->row_spacing;
  return y;
}

/* Build a server-list table whose top is at `y`, horizontally centered on the
 * viewport: a bold header row (Name | IP | Port | Players | Connect), then one
 * row per server with text cells and a diamond-gem Connect button filling the
 * last column. Every widget is registered in `owner` for one-shot cleanup. The
 * per-row Connect buttons are returned in connect_btn[] (parallel to rows[]) so
 * the caller can hit-test them with diamond_button_hit(); only the diamond
 * region (not its bounding box) is the click target. Returns the number of data
 * rows placed.
 *
 * No password indicator: a server password grants admin/bot privileges and does
 * NOT gate joining, so a lock here would mislead. in_progress is informational
 * (observers may still join) and is marked lightly on the Players cell. */
static int server_table_build(UITable_t *t, UIRegistry_t *owner, Font_t *font, int y,
                              const ServerRow_t *rows, int n, DiamondButtonWidget_t **connect_btn) {
  const SDL_Color connect_gem = {200, 35, 45, 255}; /* ruby red, card-suit diamond */

  ui_table_begin(t, 0, y, SERVER_TABLE_COLS);
  /* All columns center-justified (col_align default 0): values and headers are
   * centered in their cells, including the Connect column. */

  static const char *const hdr[SERVER_TABLE_COLS] = {
      "Name", "IP", "Port", "Players", "Uptime (days)", "Ping", "Connect"};
  for (int c = 0; c < SERVER_TABLE_COLS; c++) {
    TextWidget_t *h = text_widget_create(_(hdr[c]), font->fonts[FONT_DEFAULT_BOLD], DC_TEXT_ON_DARK);
    if (h) {
      ui_register(owner, &h->base);
      ui_table_add(t, 0, c, &h->base);
    }
  }

  int placed = 0;
  for (int i = 0; i < n; i++) {
    const ServerRow_t *s = &rows[i];
    const char *who = (s->name && s->name[0] != '\0') ? s->name : s->ip;
    char portbuf[8];
    char plbuf[16];
    char uptimebuf[16];
    char pingbuf[16];
    snprintf(portbuf, sizeof(portbuf), "%u", (unsigned)s->port);
    snprintf(plbuf, sizeof(plbuf), "%u/%u%s", (unsigned)s->player_count, (unsigned)s->max_players,
             s->in_progress ? " *" : "");
    /* Uptime in days to four decimals (~8.6 s resolution, so it visibly ticks
     * on each ~10 s registry refresh), derived from the server's reported boot
     * instant against our own wall clock. start_time == 0 means unknown (LAN
     * rows, or an older server that predates the field); a future/garbled
     * start_time (now < start_time, e.g. clock skew) is also shown as "-". */
    time_t now = time(NULL);
    if (s->start_time != 0 && (uint64_t)now >= s->start_time)
      snprintf(uptimebuf, sizeof(uptimebuf), "%.4f", (double)((uint64_t)now - s->start_time) / 86400.0);
    else
      snprintf(uptimebuf, sizeof(uptimebuf), "-");

    /* Ping cell: the ms number when measured, "-" while pending (and for LAN
     * rows before the prober has reached them), "x" when unreachable. */
    if (s->ping_ms == PING_UNREACHABLE)
      snprintf(pingbuf, sizeof(pingbuf), "x");
    else if (s->ping_ms == PING_PENDING)
      snprintf(pingbuf, sizeof(pingbuf), "-");
    else
      snprintf(pingbuf, sizeof(pingbuf), "%d", s->ping_ms);

    const char *cells[SERVER_TABLE_COLS - 1] = {who, s->ip, portbuf, plbuf, uptimebuf, pingbuf};
    int row = i + 1;
    for (int c = 0; c < SERVER_TABLE_COLS - 1; c++) {
      TextWidget_t *tw = text_widget_create(cells[c], font->fonts[FONT_DEFAULT], DC_TEXT_ON_DARK);
      if (tw) {
        ui_register(owner, &tw->base);
        ui_table_add(t, row, c, &tw->base);
      }
    }
    DiamondButtonWidget_t *cb = diamond_button_create(CONNECT_BTN_W, CONNECT_BTN_H, connect_gem);
    if (cb) {
      ui_register(owner, &cb->base);
      ui_table_add(t, row, SERVER_TABLE_COLS - 1, &cb->base);
    }
    connect_btn[placed++] = cb;
  }

  /* Center the whole table on the viewport now that column widths are known. */
  t->x = g_viewport.x + (g_viewport.w - server_table_width(t)) / 2;
  ui_table_layout(t);
  return placed;
}

/* Draw a styled backdrop for a laid-out table: a translucent panel, a darker
 * header band, faint zebra striping, a header underline, and a border. Call
 * before rendering the cells so the text and buttons land on top. */
static void server_table_draw_style(const UITable_t *t, SDL_Renderer *r) {
  /* Shared with the lobby player list; see ui_table_draw_styled_backdrop. */
  ui_table_draw_styled_backdrop(t, r);
}

/* The blocking registry query (connect + list round trip per registry) now runs
 * on a dedicated worker thread in registry_fetch_thread.c, so a slow/unreachable
 * registry no longer stalls this ~60fps connect screen (#82). The UI thread only
 * reads the worker's latest published list each frame. */

/* Translucent rounded pill behind a text widget so it reads on the felt. */
static void draw_pill_behind(SDL_Renderer *r, SDL_Rect wr, int padx, int pady, SDL_Color c) {
  const SDL_Rect plate = {wr.x - padx, wr.y - pady, wr.w + 2 * padx, wr.h + 2 * pady};
  draw_round_rect(r, plate, plate.h / 2, c);
}

int menu_display_connect(PlayerConfig_t *player_config, char *host_str, uint16_t *port,
                                SdlContext_t *sdl_context, Font_t *font, LinkWidget_t **links) {
  ButtonWidget_t *button_connect =
      button_widget_create_styled(_("Connect"), &ROLE_PRIMARY, font->fonts, (SDL_Keycode)0);
  ButtonWidget_t *button_settings =
      button_widget_create_styled(_("Settings"), &ROLE_PRIMARY, font->fonts, (SDL_Keycode)0);
  UIRegistry_t reg = {0};

  /* Settings button: right margin (top area). */
  button_settings->base.rect.x =
      g_viewport.x + g_viewport.w - button_settings->base.rect.w - g_layout_cfg.margin;
  button_settings->base.rect.y = g_layout.menu.connect_btn_y;

  int input_w;
  if (TTF_SizeUTF8(font->fonts[FONT_DEFAULT], "255.255.255.255", &input_w, NULL) != 0)
    input_w = 150;
  input_w += g_layout_cfg.connect_input_w_pad;
  /* The full IPv4 width was wider than needed in practice; trim ~20%. Text still
     scrolls within the field if a longer host is typed. */
  input_w = input_w * 4 / 5;
  /* Hostnames are often longer than an IPv4 literal; give the host field ~2x. */
  const int host_w = input_w * 2;

  const int lbl_pad = g_layout_cfg.connect_input_w_pad;

  /* Manual-entry table (settings style): a dark panel with a "nick" row and a
     "host" + "port" row. Light labels read on the dark panel. */
  TextWidget_t *tw_nick = text_widget_create(_("nick:"), font->fonts[FONT_DEFAULT], DC_TEXT_ON_DARK);
  TextWidget_t *tw_host = text_widget_create(_("host:"), font->fonts[FONT_DEFAULT], DC_TEXT_ON_DARK);
  TextWidget_t *tw_port = text_widget_create(_("port:"), font->fonts[FONT_DEFAULT], DC_TEXT_ON_DARK);
  const int nick_lw = tw_nick ? tw_nick->base.rect.w : 0;
  const int host_lw = tw_host ? tw_host->base.rect.w : 0;
  const int port_lw = tw_port ? tw_port->base.rect.w : 0;
  const int col_lw = (nick_lw > host_lw) ? nick_lw : host_lw; /* nick & host share a column */

  InputWidget_t *nick_input =
      input_widget_create(player_config->nick, font->fonts[FONT_DEFAULT], input_w, CFG_TYPE_STRING);
  if (!nick_input)
    goto err;
  nick_input->max_len = SIZEOF_NICK - 1;

  InputWidget_t *host_input =
      input_widget_create(host_str, font->fonts[FONT_DEFAULT], host_w, CFG_TYPE_STRING);
  if (!host_input)
    goto err;
  host_input->focused = true;

  char port_init[16] = {0};
  snprintf(port_init, sizeof(port_init), "%u", (unsigned)*port);
  InputWidget_t *port_input =
      input_widget_create(port_init, font->fonts[FONT_DEFAULT], input_w, CFG_TYPE_UINT16);
  if (!port_input)
    goto err;

  const int panel_pad = 18;
  const int row_h = host_input->base.rect.h;
  const int row_gap = g_layout_cfg.input_field_v_gap;
  const int panel_x = g_layout.menu.margin_x;
  const int content_x = panel_x + panel_pad;
  const int field_x = content_x + col_lw + lbl_pad;
  const int port_label_x = field_x + host_w + 40;
  const int port_field_x = port_label_x + port_lw + lbl_pad;
  const int row0_y = g_layout.menu.connect_host_y;        /* nick row */
  const int row1_y = row0_y + row_h + row_gap;            /* host + port row */

  if (tw_nick)
    ui_widget_place(&tw_nick->base, content_x, row0_y + 4);
  nick_input->base.rect.x = field_x;
  nick_input->base.rect.y = row0_y;
  ui_register(&reg, &nick_input->base);

  if (tw_host)
    ui_widget_place(&tw_host->base, content_x, row1_y + 4);
  host_input->base.rect.x = field_x;
  host_input->base.rect.y = row1_y;
  ui_register(&reg, &host_input->base);

  if (tw_port)
    ui_widget_place(&tw_port->base, port_label_x, row1_y + 4);
  port_input->base.rect.x = port_field_x;
  port_input->base.rect.y = row1_y;
  ui_register(&reg, &port_input->base);

  /* Panel rect behind the rows (drawn in the render loop). */
  const SDL_Rect input_panel = {panel_x, row0_y - panel_pad,
                                (port_field_x + input_w + panel_pad) - panel_x,
                                (row1_y + row_h + panel_pad) - (row0_y - panel_pad)};

  /* Action buttons to the right of the table, vertically centered on it:
     Connect, then Save / Load Defaults. */
  const int action_x = input_panel.x + input_panel.w + 30;
  const int action_y = input_panel.y + (input_panel.h - button_connect->base.rect.h) / 2;
  button_connect->base.rect.x = action_x;
  button_connect->base.rect.y = action_y;

  ButtonWidget_t *button_save =
      button_widget_create_styled(_("Save"), &ROLE_PRIMARY, font->fonts, (SDL_Keycode)0);
  if (!button_save)
    goto err;
  button_save->interactive = false;
  button_save->base.rect.x =
      button_connect->base.rect.x + button_connect->base.rect.w + g_layout_cfg.connect_save_btn_gap;
  button_save->base.rect.y = action_y;
  ui_register(&reg, &button_save->base);

  ButtonWidget_t *button_defaults =
      button_widget_create_styled(_("Load Defaults"), &ROLE_PRIMARY, font->fonts, (SDL_Keycode)0);
  if (!button_defaults)
    goto err;
  button_defaults->base.rect.x =
      button_save->base.rect.x + button_save->base.rect.w + g_layout_cfg.connect_save_btn_gap;
  button_defaults->base.rect.y = action_y;
  ui_register(&reg, &button_defaults->base);

  ButtonWidget_t *btn_quit_connect =
      button_widget_create_styled("X", &ROLE_DANGER, font->fonts, (SDL_Keycode)0);
  if (btn_quit_connect) {
    btn_quit_connect->base.rect.x =
        g_viewport.x + g_viewport.w - btn_quit_connect->base.rect.w - g_layout_cfg.margin;
    btn_quit_connect->base.rect.y = g_layout.menu.quit_y;
    ui_register(&reg, &btn_quit_connect->base);
  }

  InputWidget_t *focused_inputs[3] = {host_input, port_input, nick_input};
  int focused_slot = 0;

  SDL_StartTextInput();

  layout_links(links, LINK_DEFS_COUNT);

  /* Black title on a translucent orange pill (drawn in the render loop) so it
     reads on the felt. */
  TextWidget_t *tw_title =
      text_widget_create(DEALERSCHOICE_FORMAL_NAME, font->fonts[FONT_TITLE], get_color(COLOR_BLACK));
  if (tw_title)
    ui_widget_place(&tw_title->base, g_layout.menu.title_x, g_layout.menu.title_y);

  /* App logo to the left of the title, vertically centered on it. */
  const int connect_logo_sz = 96;
  /* Gap clears the title's orange pill (padx 22) plus breathing room. */
  SDL_Rect connect_logo_dst = {g_layout.menu.title_x - connect_logo_sz - 46,
                               g_layout.menu.title_y, connect_logo_sz, connect_logo_sz};
  if (tw_title)
    connect_logo_dst.y = tw_title->base.rect.y + (tw_title->base.rect.h - connect_logo_sz) / 2;

  char version[64] = {0};
  snprintf(version, sizeof(version), "Version " DEALERSCHOICE_VERSION);
  TextWidget_t *tw_version =
      text_widget_create(version, font->fonts[FONT_VERSION], DC_TEXT_ON_DARK);
  if (tw_version)
    ui_widget_place(&tw_version->base, g_layout.menu.title_x + g_layout_cfg.version_x_offset,
                    g_layout.menu.title_y + g_layout_cfg.version_y_offset);


  /* Heading above the discovered-server rows; only drawn when at least one
   * server is found. */
  int lan_heading_y = input_panel.y + input_panel.h + g_layout_cfg.input_field_v_gap;
  TextWidget_t *tw_lan_heading =
      text_widget_create(_("Servers on LAN"), font->fonts[FONT_DEFAULT_BOLD], DC_TEXT_ON_DARK);
  if (tw_lan_heading)
    ui_widget_place(&tw_lan_heading->base, g_layout.menu.margin_x, lan_heading_y);

  bool run_client = false;
  bool run_settings = false;
  bool running = true;

  /* LAN discovery: passively look for games on the network and show them as
   * clickable rows below the nick. Clicking a row connects to it immediately.
   * Optional — if a socket can't be opened, the screen just works manually.
   * Two sockets share one set: IPv4 (broadcast) + IPv6 (link-local multicast);
   * responses from both merge into found[] (deduped by server instance_id in
   * lan_upsert, so one server isn't listed once per interface/address). */
  tcpme_socket_t disc_sock = lan_discovery_open_client();
  tcpme_socket_t disc_sock6 = lan_discovery_open_client6();
  tcpme_set_t *disc_set = NULL;
  if (tcpme_socket_valid(disc_sock) || tcpme_socket_valid(disc_sock6)) {
    disc_set = tcpme_alloc_set(2);
    if (disc_set) {
      if (tcpme_socket_valid(disc_sock))
        tcpme_add_socket(disc_set, disc_sock);
      if (tcpme_socket_valid(disc_sock6))
        tcpme_add_socket(disc_set, disc_sock6);
    }
    if (tcpme_socket_valid(disc_sock))
      lan_discovery_query(disc_sock, player_config->lan_discovery_port);
    if (tcpme_socket_valid(disc_sock6))
      lan_discovery_query6(disc_sock6);
  }
  uint32_t last_query = SDL_GetTicks();
  LanGameInfo_t found[LAN_MAX_SHOWN];
  uint32_t found_seen[LAN_MAX_SHOWN];
  int found_count = 0;
  uint64_t shown_sig = 0;

  /* Server lists are stacked, centered tables: "Internet servers" on top,
   * "Servers on LAN" below, with a divider between (#33). server_table_build()
   * centers each table horizontally; the headings are centered to match and
   * placed on (re)build. */
  const int servers_top_y = lan_heading_y;
  const int section_gap = g_layout_cfg.input_field_v_gap * 3;

  /* Internet servers (top) from the configured registries, slow-refreshed.
   * registry_browser (player.conf / --disable-registry-browser) lets the user
   * opt out of querying the registry entirely. */
  const bool browse_registry = (player_config->registry_count > 0 && player_config->registry_browser);
  RegistryServer_t reg_found[REG_MAX_SHOWN];
  int reg_found_count = 0;
  /* The background fetcher owns the blocking registry query (#82). We poll its
   * published list every frame; reg_fetch_version tracks the last result we've
   * folded into reg_found[] so the table rebuilds only on a new fetch. NULL is
   * fine (no registry rows shown). The fetcher is destroyed before we leave this
   * function, before the widgets/state it has nothing to do with are torn down,
   * mirroring the ping prober. */
  RegistryFetcher_t *reg_fetcher = browse_registry ? registry_fetch_create(player_config) : NULL;
  uint32_t reg_fetch_version = 0;
  UITable_t reg_table = {0};
  UIRegistry_t reg_tbl_reg = {0};
  DiamondButtonWidget_t *reg_connect[REG_MAX_SHOWN] = {0};
  int reg_connect_count = 0;
  int reg_bottom = servers_top_y; /* visual bottom of the Internet section */
  int prev_reg_bottom = -1;       /* the LAN table relayouts when this moves */
  uint64_t prev_reg_ping_sig = 0; /* rebuild the table when ping values change */
  TextWidget_t *tw_reg_heading =
      text_widget_create(_("Internet servers"), font->fonts[FONT_DEFAULT_BOLD], DC_TEXT_ON_DARK);

  /* LAN servers (bottom). */
  UITable_t lan_table = {0};
  UIRegistry_t lan_tbl_reg = {0};
  DiamondButtonWidget_t *lan_connect[LAN_MAX_SHOWN] = {0};
  int lan_connect_count = 0;

  bool show_keys_overlay = false; /* F1 "Keys" reference panel */

  /* Background latency prober for both server tables (#98). It probes off the
   * UI thread; we feed it the current host:port set whenever a table rebuilds
   * and read a per-row value back at build time. NULL is fine (no ping data
   * shown). It is destroyed before we leave this function. */
  PingProbe_t *ping_probe = ping_probe_create();
  uint64_t prev_targets_sig = 0; /* host:port set last handed to the prober */

  while (running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      /* F1 toggles the read-only key list; while it is open it swallows other
       * keys (so Esc closes the panel rather than leaving the screen). */
      if (hotkey_overlay_handle_event(&e, &show_keys_overlay))
        continue;
      SDL_Point mouse_pos = {e.button.x, e.button.y};
      button_connect->base.hovered = SDL_PointInRect(&mouse_pos, &button_connect->base.rect);
      button_settings->base.hovered = SDL_PointInRect(&mouse_pos, &button_settings->base.rect);
      button_save->base.hovered = SDL_PointInRect(&mouse_pos, &button_save->base.rect);
      button_defaults->base.hovered = SDL_PointInRect(&mouse_pos, &button_defaults->base.rect);
      if (btn_quit_connect)
        btn_quit_connect->base.hovered = SDL_PointInRect(&mouse_pos, &btn_quit_connect->base.rect);
      for (size_t i = 0; i < LINK_DEFS_COUNT; i++)
        links[i]->base.hovered = SDL_PointInRect(&mouse_pos, &links[i]->base.rect);
      /* Hover uses the diamond region, not the bounding rect, so the empty
       * triangular corners of the cell don't light the gem (#93). */
      for (int i = 0; i < lan_connect_count; i++)
        if (lan_connect[i])
          lan_connect[i]->base.hovered = diamond_button_hit(lan_connect[i], mouse_pos.x, mouse_pos.y);
      for (int i = 0; i < reg_connect_count; i++)
        if (reg_connect[i])
          reg_connect[i]->base.hovered = diamond_button_hit(reg_connect[i], mouse_pos.x, mouse_pos.y);
      if (e.type == SDL_QUIT) {
        running = false;
      } else if (e.type == SDL_MOUSEBUTTONDOWN) {
        if (btn_quit_connect && SDL_PointInRect(&mouse_pos, &btn_quit_connect->base.rect) &&
            confirm_quit(font->fonts)) {
          SDL_Event quit = {.type = SDL_QUIT};
          SDL_PushEvent(&quit);
          running = false;
        } else if (SDL_PointInRect(&mouse_pos, &button_connect->base.rect)) {
          run_client = true;
          running = false;
        } else if (SDL_PointInRect(&mouse_pos, &button_settings->base.rect)) {
          run_settings = true;
          running = false;
        } else if (button_save->interactive &&
                   SDL_PointInRect(&mouse_pos, &button_save->base.rect)) {
          button_save->click.start_time = SDL_GetTicks();
          player_config_set_field(player_config, 0, input_widget_get_text(nick_input));
          player_config_set_field(player_config, 1, input_widget_get_text(host_input));
          player_config_set_field(player_config, 2, input_widget_get_text(port_input));
          save_player_config(player_config);
        } else if (SDL_PointInRect(&mouse_pos, &button_defaults->base.rect)) {
          button_defaults->click.start_time = SDL_GetTicks();
          input_widget_set_text(host_input, player_config_entries[1].default_value);
          input_widget_set_text(port_input, player_config_entries[2].default_value);
        } else if (SDL_PointInRect(&mouse_pos, &host_input->base.rect)) {
          focused_inputs[focused_slot]->focused = false;
          focused_slot = 0;
          focused_inputs[focused_slot]->focused = true;
        } else if (SDL_PointInRect(&mouse_pos, &port_input->base.rect)) {
          focused_inputs[focused_slot]->focused = false;
          focused_slot = 1;
          focused_inputs[focused_slot]->focused = true;
        } else if (SDL_PointInRect(&mouse_pos, &nick_input->base.rect)) {
          focused_inputs[focused_slot]->focused = false;
          focused_slot = 2;
          focused_inputs[focused_slot]->focused = true;
        } else {
          for (int i = 0; i < lan_connect_count; i++) {
            if (lan_connect[i] && diamond_button_hit(lan_connect[i], mouse_pos.x, mouse_pos.y)) {
              input_widget_set_text(host_input, found[i].ip);
              char pbuf[8];
              snprintf(pbuf, sizeof(pbuf), "%u", (unsigned)found[i].tcp_port);
              input_widget_set_text(port_input, pbuf);
              run_client = true;
              running = false;
              break;
            }
          }
          for (int i = 0; i < reg_connect_count && running; i++) {
            if (reg_connect[i] && diamond_button_hit(reg_connect[i], mouse_pos.x, mouse_pos.y)) {
              input_widget_set_text(host_input, reg_found[i].ip);
              char pbuf[8];
              snprintf(pbuf, sizeof(pbuf), "%u", (unsigned)reg_found[i].tcp_port);
              input_widget_set_text(port_input, pbuf);
              run_client = true;
              running = false;
              break;
            }
          }
        }
        for (size_t i = 0; i < LINK_DEFS_COUNT; i++) {
          if (links[i]->base.hovered && e.button.button == SDL_BUTTON_LEFT)
            if (SDL_OpenURL(links[i]->url) == -1)
              fputs(SDL_GetError(), stderr);
        }
      } else if (e.type == SDL_TEXTINPUT) {
        input_widget_append(focused_inputs[focused_slot], e.text.text);
      } else if (e.type == SDL_KEYDOWN) {
        switch (e.key.keysym.sym) {
        case SDLK_ESCAPE:
          if (confirm_quit(font->fonts)) {
            SDL_Event quit = {.type = SDL_QUIT};
            SDL_PushEvent(&quit);
            running = false;
          }
          break;

        case SDLK_RETURN:
          /* Plain Enter does NOT connect: it would fire accidentally while
             editing a field (e.g. after typing a nick). Use the Connect button.
             Alt+Enter still toggles fullscreen. */
          if (e.key.keysym.mod & KMOD_ALT)
            toggle_fullscreen(sdl_context);
          break;

        case SDLK_F11:
          toggle_fullscreen(sdl_context);
          break;

        case SDLK_TAB: {
          focused_inputs[focused_slot]->focused = false;
          int dir = (e.key.keysym.mod & KMOD_SHIFT) ? -1 : 1;
          focused_slot = (focused_slot + dir + 3) % 3;
          focused_inputs[focused_slot]->focused = true;
          break;
        }

        case SDLK_BACKSPACE:
          input_widget_backspace(focused_inputs[focused_slot]);
          break;

        case SDLK_LEFT:
          input_widget_cursor_left(focused_inputs[focused_slot]);
          break;

        case SDLK_RIGHT:
          input_widget_cursor_right(focused_inputs[focused_slot]);
          break;

        case SDLK_HOME:
          input_widget_cursor_home(focused_inputs[focused_slot]);
          break;

        case SDLK_END:
          input_widget_cursor_end(focused_inputs[focused_slot]);
          break;

        case SDLK_v:
          if (e.key.keysym.mod & KMOD_CTRL) {
            char *clip = SDL_GetClipboardText();
            if (clip) {
              input_widget_append(focused_inputs[focused_slot], clip);
              SDL_free(clip);
            }
          }
          break;

        default:
          break;
        }
      }
    }

    /* --- Internet servers (top): slow-refresh the registry table, anchored at
     * the top of the server-list area (#33). --- */
    if (reg_fetcher) {
      /* Pull the worker's latest published list (a cheap mutex-guarded copy; the
       * blocking query happens on its thread). A bumped version means a fetch
       * completed, so re-copy into reg_found[]. Also rebuild when the prober has
       * new latency values for the rows we already show, so the Ping column
       * updates without waiting for the next registry fetch. */
      RegistryServer_t latest[REG_MAX_SHOWN];
      int latest_count = 0;
      uint32_t fetch_ver = registry_fetch_get(reg_fetcher, latest, REG_MAX_SHOWN, &latest_count);
      bool reg_fetch_due = (fetch_ver != reg_fetch_version);

      uint64_t reg_ping_sig = 0;
      for (int i = 0; i < reg_found_count; i++)
        reg_ping_sig = reg_ping_sig * 1099511628211ULL +
                       (uint64_t)(uint32_t)ping_probe_get(ping_probe, reg_found[i].ip,
                                                          reg_found[i].tcp_port);
      bool reg_ping_changed = (reg_ping_sig != prev_reg_ping_sig);
      if (reg_fetch_due || reg_ping_changed) {
        if (reg_fetch_due) {
          memcpy(reg_found, latest, sizeof(RegistryServer_t) * (size_t)latest_count);
          reg_found_count = latest_count;
          reg_fetch_version = fetch_ver;
        }
        prev_reg_ping_sig = reg_ping_sig;

        const int reg_heading_h = tw_reg_heading ? tw_reg_heading->base.rect.h : 0;
        ui_destroy_all(&reg_tbl_reg);
        reg_connect_count = 0;
        if (reg_found_count > 0) {
          ServerRow_t rows[REG_MAX_SHOWN];
          for (int i = 0; i < reg_found_count; i++) {
            rows[i] = (ServerRow_t){reg_found[i].name,         reg_found[i].ip,
                                    reg_found[i].tcp_port,     reg_found[i].player_count,
                                    reg_found[i].max_players,  reg_found[i].in_progress,
                                    reg_found[i].start_time,   PING_PENDING};
            rows[i].ping_ms = ping_probe_get(ping_probe, reg_found[i].ip, reg_found[i].tcp_port);
          }
          reg_connect_count = server_table_build(
              &reg_table, &reg_tbl_reg, font,
              servers_top_y + reg_heading_h + g_layout_cfg.input_field_v_gap, rows, reg_found_count,
              reg_connect);
          if (tw_reg_heading)
            ui_widget_place(&tw_reg_heading->base,
                            g_viewport.x + (g_viewport.w - tw_reg_heading->base.rect.w) / 2,
                            servers_top_y);
          reg_bottom = server_table_bottom(&reg_table);
        } else {
          reg_bottom = servers_top_y; /* nothing shown; LAN starts at the top */
        }
      }
    }

    /* --- Servers on LAN (bottom): re-query, drain replies, expire, then rebuild
     * below the Internet table; also relayout when the table above moves. --- */
    if (disc_set) {
      uint32_t now = SDL_GetTicks();
      if (now - last_query >= 2000) {
        if (tcpme_socket_valid(disc_sock))
          lan_discovery_query(disc_sock, player_config->lan_discovery_port);
        if (tcpme_socket_valid(disc_sock6))
          lan_discovery_query6(disc_sock6);
        last_query = now;
      }
      /* Drain replies from both family sockets; merge into found[] (lan_upsert
       * dedups by instance_id, so one server reachable on many addresses
       * (IPv4 + several IPv6 link-local + loopback) shows as a single row). */
      int drain = 0;
      while (drain++ < 32 && tcpme_check_sockets(disc_set, 0) > 0) {
        bool got = false;
        LanGameInfo_t g;
        if (tcpme_socket_valid(disc_sock) && tcpme_socket_ready(disc_set, disc_sock) &&
            lan_discovery_read_response(disc_sock, &g)) {
          found_count = lan_upsert(found, found_seen, found_count, LAN_MAX_SHOWN, &g, now);
          got = true;
        }
        if (tcpme_socket_valid(disc_sock6) && tcpme_socket_ready(disc_set, disc_sock6) &&
            lan_discovery_read_response6(disc_sock6, &g)) {
          found_count = lan_upsert(found, found_seen, found_count, LAN_MAX_SHOWN, &g, now);
          got = true;
        }
        if (!got)
          break; /* select fired but nothing parseable this pass */
      }
      found_count = lan_expire(found, found_seen, found_count, now, 6000);

      /* Fold the current ping values into the change signature so the LAN table
       * rebuilds (and the Ping column updates) when the prober reports new
       * latencies, not only when the discovered set changes. */
      uint64_t sig = lan_list_sig(found, found_count);
      for (int i = 0; i < found_count; i++)
        sig = sig * 1099511628211ULL +
              (uint64_t)(uint32_t)ping_probe_get(ping_probe, found[i].ip, found[i].tcp_port);
      bool moved = (reg_bottom != prev_reg_bottom);
      if (sig != shown_sig || moved) {
        const int lan_top = (reg_connect_count > 0) ? reg_bottom + section_gap : servers_top_y;
        const int lan_heading_h = tw_lan_heading ? tw_lan_heading->base.rect.h : 0;
        ui_destroy_all(&lan_tbl_reg);
        lan_connect_count = 0;
        if (found_count > 0) {
          ServerRow_t rows[LAN_MAX_SHOWN];
          for (int i = 0; i < found_count; i++) {
            /* LAN discovery carries no start_time, so uptime is unknown (0 ->
             * the table shows "-"). */
            rows[i] = (ServerRow_t){found[i].name,         found[i].ip,
                                    found[i].tcp_port,      found[i].player_count,
                                    found[i].max_players,   found[i].in_progress,
                                    0,                      PING_PENDING};
            rows[i].ping_ms = ping_probe_get(ping_probe, found[i].ip, found[i].tcp_port);
          }
          lan_connect_count =
              server_table_build(&lan_table, &lan_tbl_reg, font,
                                 lan_top + lan_heading_h + g_layout_cfg.input_field_v_gap, rows,
                                 found_count, lan_connect);
          if (tw_lan_heading)
            ui_widget_place(&tw_lan_heading->base,
                            g_viewport.x + (g_viewport.w - tw_lan_heading->base.rect.w) / 2,
                            lan_top);
        }
        shown_sig = sig;
        prev_reg_bottom = reg_bottom;
      }
    }

    /* Feed the prober the union of currently-shown registry + LAN servers. Only
     * push when the host:port set changes (ping_probe_set_targets bumps the
     * worker's generation, which forces a re-probe), so we don't restart the
     * probe cycle every frame. The signature is over host:port only. */
    if (ping_probe) {
      const char *thosts[REG_MAX_SHOWN + LAN_MAX_SHOWN];
      uint16_t tports[REG_MAX_SHOWN + LAN_MAX_SHOWN];
      int tcount = 0;
      uint64_t tsig = 1469598103934665603ULL;
      for (int i = 0; i < reg_found_count; i++) {
        thosts[tcount] = reg_found[i].ip;
        tports[tcount] = reg_found[i].tcp_port;
        tcount++;
      }
      for (int i = 0; i < found_count; i++) {
        thosts[tcount] = found[i].ip;
        tports[tcount] = found[i].tcp_port;
        tcount++;
      }
      for (int i = 0; i < tcount; i++) {
        for (const char *c = thosts[i]; *c; c++)
          tsig = (tsig ^ (unsigned char)*c) * 1099511628211ULL;
        tsig = (tsig ^ tports[i]) * 1099511628211ULL;
      }
      if (tsig != prev_targets_sig) {
        ping_probe_set_targets(ping_probe, thosts, tports, tcount);
        prev_targets_sig = tsig;
      }
    }

    button_save->interactive =
        strcmp(input_widget_get_text(host_input), player_config->host) != 0 ||
        strcmp(input_widget_get_text(nick_input), player_config->nick) != 0 ||
        (uint16_t)strtoul(input_widget_get_text(port_input), NULL, 10) != player_config->port;

    clear_screen(sdl_context->renderer);
    /* Dark table panel behind the nick/host/port rows. */
    draw_round_rect(sdl_context->renderer, input_panel, 18, (SDL_Color){0, 0, 0, 150});
    ui_widget_render(&button_connect->base);
    ui_widget_render(&button_settings->base);
    ui_render_all(&reg);

    if (reg_connect_count > 0) {
      server_table_draw_style(&reg_table, sdl_context->renderer);
      if (tw_reg_heading)
        ui_widget_render(&tw_reg_heading->base);
      ui_render_all(&reg_tbl_reg);
    }

    if (lan_connect_count > 0) {
      server_table_draw_style(&lan_table, sdl_context->renderer);
      if (tw_lan_heading)
        ui_widget_render(&tw_lan_heading->base);
      ui_render_all(&lan_tbl_reg);
    }

    /* Divider between the Internet and LAN sections when both are shown. */
    if (reg_connect_count > 0 && lan_connect_count > 0) {
      int wr = server_table_width(&reg_table);
      int wl = server_table_width(&lan_table);
      int sw = (wr > wl ? wr : wl);
      SDL_Rect sep = {g_viewport.x + (g_viewport.w - sw) / 2, reg_bottom + section_gap / 2, sw, 2};
      SDL_SetRenderDrawBlendMode(sdl_context->renderer, SDL_BLENDMODE_BLEND);
      SDL_SetRenderDrawColor(sdl_context->renderer, 255, 255, 255, 120);
      SDL_RenderFillRect(sdl_context->renderer, &sep);
    }

    draw_logo(sdl_context->renderer, connect_logo_dst);
    if (tw_nick)
      ui_widget_render(&tw_nick->base);
    if (tw_host)
      ui_widget_render(&tw_host->base);
    if (tw_port)
      ui_widget_render(&tw_port->base);
    {
      SDL_Color title_pill = get_color(COLOR_ORANGE);
      title_pill.a = 64;
      draw_pill_behind(sdl_context->renderer, tw_title->base.rect, 22, 8, title_pill);
    }
    ui_widget_render(&tw_title->base);
    ui_widget_render(&tw_version->base);

    for (size_t i = 0; i < LINK_DEFS_COUNT; i++)
      ui_widget_render(&links[i]->base);

    if (show_keys_overlay)
      hotkey_overlay_render(sdl_context->renderer, font, false);

    SDL_RenderPresent(sdl_context->renderer);
    SDL_Delay(16);
  }

  SDL_StopTextInput();

  /* Stop and join the background workers before tearing down anything they might
   * touch. Each *_destroy signals its stop flag and joins, so no detached thread
   * is left to use the (about-to-be-freed) shared state. The two workers are
   * independent (separate mutexes/state); order between them doesn't matter. */
  ping_probe_destroy(ping_probe);
  registry_fetch_destroy(reg_fetcher);

  /* Copy final values back to the caller's variables */
  const char *final_host = input_widget_get_text(host_input);
  strncpy(host_str, final_host, MAX_INPUT_LENGTH - 1);
  host_str[MAX_INPUT_LENGTH - 1] = '\0';

  const char *final_port = input_widget_get_text(port_input);
  if (final_port && *final_port)
    *port = (uint16_t)strtoul(final_port, NULL, 10);

  /* Apply the edited nick to the live config so this session connects under it
   * even if it wasn't persisted with Save. */
  const char *final_nick = input_widget_get_text(nick_input);
  if (final_nick && *final_nick)
    player_config_set_field(player_config, 0, final_nick);

  if (tw_title)
    ui_widget_destroy(&tw_title->base);
  if (tw_version)
    ui_widget_destroy(&tw_version->base);
  if (tw_nick)
    ui_widget_destroy(&tw_nick->base);
  if (tw_host)
    ui_widget_destroy(&tw_host->base);
  if (tw_port)
    ui_widget_destroy(&tw_port->base);
  if (tw_lan_heading)
    ui_widget_destroy(&tw_lan_heading->base);
  if (tw_reg_heading)
    ui_widget_destroy(&tw_reg_heading->base);
  ui_widget_destroy(&button_connect->base);
  ui_widget_destroy(&button_settings->base);
  ui_destroy_all(&reg);
  ui_destroy_all(&lan_tbl_reg);
  ui_destroy_all(&reg_tbl_reg);
  if (disc_set)
    tcpme_free_set(disc_set);
  if (tcpme_socket_valid(disc_sock))
    tcpme_close(disc_sock);
  if (tcpme_socket_valid(disc_sock6))
    tcpme_close(disc_sock6);

  if (run_client)
    return RUN_CLIENT;
  if (run_settings)
    return RUN_SETTINGS;
  return 0;

err:
  ui_widget_destroy(&button_connect->base);
  ui_widget_destroy(&button_settings->base);
  /* tw_nick/tw_host/tw_port are not registered in `reg` (they're placed
     manually, like the two top buttons), so ui_destroy_all(&reg) won't free
     them — destroy them here too, matching the normal-exit cleanup above. */
  if (tw_nick)
    ui_widget_destroy(&tw_nick->base);
  if (tw_host)
    ui_widget_destroy(&tw_host->base);
  if (tw_port)
    ui_widget_destroy(&tw_port->base);
  ui_destroy_all(&reg);
  return 0;
}

/* Keys the in-game loop already owns or that are kept for app-level use;
 * binding an action to one would shadow it.  Digits 1-8 are the bet-amount
 * buttons; the whole F1-F12 row is reserved (F11 fullscreen, F1 the planned
 * in-game hotkey overlay, the rest held for future shortcuts). */
static bool is_reserved_hotkey(SDL_Keycode k) {
  switch (k) {
  case SDLK_ESCAPE:
  case SDLK_RETURN:
  case SDLK_KP_ENTER:
  case SDLK_TAB:
    return true;
  default:
    return (k >= SDLK_1 && k <= SDLK_8) || (k >= SDLK_F1 && k <= SDLK_F12);
  }
}

/* The configurable hotkeys are exactly the player-config entries whose key is
 * prefixed "hotkey_"; they are shown on the Hotkeys sub-screen, not the main
 * Settings grid. */
static bool is_hotkey_entry(size_t i) {
  return strncmp(player_config_entries[i].key, "hotkey_", 7) == 0;
}

/* Group-aware conflict test for two action hotkeys.
 *
 * Two actions may safely share a key only when the in-game dispatch
 * (src/ui/game_logic.c) never offers them in the same betting mode.  The modes
 * are mutually exclusive:
 *   bet_check_fold      -> BET / CHECK / FOLD
 *   call_raise_fold     -> CALL / RAISE / FOLD
 *   call_complete_fold  -> CALL / COMPLETE / FOLD
 *   complete_check_fold -> COMPLETE / CHECK / FOLD
 * so e.g. CHECK and CALL never coexist (check shares a key with call freely),
 * and RAISE and COMPLETE never coexist.  Everything that *can* be live together
 * must stay distinct, or one keypress would fire two actions.
 *
 * discard and hand_rank are not betting actions: discard is only active during
 * the draw phase, but hand_rank is checked unconditionally on every keydown, so
 * it could fire alongside any betting action.  To stay safe, both conflict with
 * every other action (the old global-uniqueness rule for them).
 *
 * `a` and `b` are the action suffixes (the part after "hotkey_"), e.g. "check".
 * Returns true if the two actions must NOT share a key. */
static bool hotkey_action_excludes(const char *a, const char *b) {
  /* Per-action conflict sets, encoded as NULL-terminated lists of the action
   * suffixes each action must differ from.  The betting pairs check<->call and
   * raise<->complete are deliberately omitted.  Every betting action also lists
   * discard and hand_rank because those two can fire alongside a betting action
   * in-game (hand_rank on every keydown, discard only during the draw — but the
   * editor can't know the phase, so keep them globally distinct). */
  static const char *const check_conflicts[] = {"bet",  "fold",      "complete",
                                                "discard", "hand_rank", NULL};
  static const char *const call_conflicts[] = {"raise", "fold",      "complete",
                                               "discard", "hand_rank", NULL};
  static const char *const bet_conflicts[] = {"check", "fold", "discard",
                                              "hand_rank", NULL};
  static const char *const fold_conflicts[] = {"bet",      "check",     "call",
                                               "raise",    "complete",  "discard",
                                               "hand_rank", NULL};
  static const char *const raise_conflicts[] = {"call", "fold", "discard",
                                                "hand_rank", NULL};
  static const char *const complete_conflicts[] = {"call",    "check",     "fold",
                                                   "discard", "hand_rank", NULL};

  static const struct {
    const char *name;
    const char *const *conflicts;
  } table[] = {
      {"check", check_conflicts},       {"call", call_conflicts},
      {"bet", bet_conflicts},           {"fold", fold_conflicts},
      {"raise", raise_conflicts},       {"complete", complete_conflicts},
  };

  for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
    if (strcmp(a, table[i].name) != 0)
      continue;
    /* `a` is a known betting action: it conflicts only with its listed set. */
    for (const char *const *c = table[i].conflicts; *c; c++)
      if (strcmp(b, *c) == 0)
        return true;
    return false;
  }

  /* `a` is discard or hand_rank (or any non-betting action): conflicts with
   * every other action. */
  return strcmp(a, b) != 0;
}

/* Symmetric wrapper: the conflict relation is mutual, so a clash exists if
 * either action excludes the other.  Checking both directions guards against an
 * accidentally one-sided table entry. */
static bool hotkeys_conflict(const char *a, const char *b) {
  return hotkey_action_excludes(a, b) || hotkey_action_excludes(b, a);
}

/* Look up the built-in default key name for a hotkey config entry.  The single
 * source of truth for defaults is g_hotkey_defs[] in hotkey_table.c, keyed by
 * the same config_key strings as player_config_entries[] — so the "Load
 * defaults" button reads from there instead of re-listing the defaults here.
 * Returns NULL if the key isn't a known configurable hotkey. */
static const char *hotkey_default_for_key(const char *config_key) {
  for (size_t i = 0; i < g_hotkey_def_count; i++)
    if (strcmp(g_hotkey_defs[i].config_key, config_key) == 0)
      return g_hotkey_defs[i].default_key;
  return NULL;
}

/* Press-to-bind editor for the hotkey entries.  Each row shows an action and
 * its current key; clicking a row captures the next keypress.  Card-selection
 * and bet-amount digit keys are intentionally not editable. */
static void menu_display_hotkeys(PlayerConfig_t *player_config, SdlContext_t *sdl_context,
                                 Font_t *font, const Path_t *path) {
  UIRegistry_t reg = {0};

  /* Back arrow (top-left). */
  char *back_img_path = canfigger_path_join(path->data, "images/arrow_back.png");
  ImageWidget_t *back_img = back_img_path ? image_widget_create(back_img_path,
                                                                g_layout_cfg.back_btn_size,
                                                                g_layout_cfg.back_btn_size)
                                          : NULL;
  free(back_img_path);
  if (back_img) {
    back_img->base.rect.x = g_layout.menu.back_img_x;
    back_img->base.rect.y = g_layout.menu.back_img_y;
    ui_register(&reg, &back_img->base);
  }

  /* One row per hotkey action: a label plus a clickable key-box (press-to-bind).
     Same dark table + pagination shell as the settings screen. */
  typedef struct {
    size_t cfg_idx;
    TextWidget_t *label;
    TextWidget_t *value;
    char keyname[SIZEOF_HOTKEY_NAME];
  } HotkeyRow_t;

  HotkeyRow_t rows[MAX_PLAYER_CONFIG_ENTRIES];
  size_t n_rows = 0;

  for (size_t i = 0; i < player_config_entry_count; i++) {
    if (!is_hotkey_entry(i))
      continue;
    const ConfigEntry *e = &player_config_entries[i];
    HotkeyRow_t *row = &rows[n_rows];
    row->cfg_idx = i;
    const char *field = (const char *)((const uint8_t *)player_config + e->offset);
    snprintf(row->keyname, sizeof(row->keyname), "%s", field);

    char label[32];
    const char *suffix = strncmp(e->key, "hotkey_", 7) == 0 ? e->key + 7 : e->key;
    snprintf(label, sizeof(label), "%s", suffix);
    if (label[0])
      label[0] = (char)toupper((unsigned char)label[0]);

    row->label = text_widget_create(label, font->fonts[FONT_DEFAULT], DC_TEXT_ON_DARK);
    if (row->label)
      ui_register(&reg, &row->label->base);
    row->value = text_widget_create(row->keyname, font->fonts[FONT_DEFAULT], DC_TEXT_ON_DARK);
    if (row->value)
      ui_register(&reg, &row->value->base);
    n_rows++;
  }

  /* Pagination (page size exceeds the fixed action count, so the pager stays
     hidden today; kept consistent with the settings screen for when it grows). */
  const int rows_per_page = 8;
  const int n_pages =
      n_rows ? (int)((n_rows + (size_t)rows_per_page - 1) / (size_t)rows_per_page) : 1;
  int page = 0;

  const int rows_shown = (n_rows < (size_t)rows_per_page) ? (int)n_rows : rows_per_page;
  const int panel_w = 1000;
  const int panel_x = g_viewport.x + (g_viewport.w - panel_w) / 2;
  const int panel_top = g_viewport.y + 200;
  const int row_h = 64;
  const int panel_h = (rows_shown > 0 ? rows_shown : 1) * row_h;
  const int label_x = panel_x + 40;
  const int box_w = 220;
  int box_h;
  TTF_SizeUTF8(font->fonts[FONT_DEFAULT], "Ag", NULL, &box_h);
  box_h += g_layout_cfg.checkbox_pad;
  const int box_x = panel_x + panel_w - box_w - 60;
  const int pager_y = panel_top + panel_h + 24;
  const int buttons_y = pager_y + 64;

  /* Pager controls. */
  ButtonWidget_t *btn_prev =
      button_widget_create_styled(_("Prev"), &ROLE_PRIMARY, font->fonts, (SDL_Keycode)0);
  ButtonWidget_t *btn_next =
      button_widget_create_styled(_("Next"), &ROLE_PRIMARY, font->fonts, (SDL_Keycode)0);
  if (btn_prev) {
    btn_prev->base.rect.x = panel_x;
    btn_prev->base.rect.y = pager_y;
    ui_register(&reg, &btn_prev->base);
  }
  if (btn_next) {
    btn_next->base.rect.x = panel_x + panel_w - btn_next->base.rect.w;
    btn_next->base.rect.y = pager_y;
    ui_register(&reg, &btn_next->base);
  }

  ButtonWidget_t *btn_save =
      button_widget_create_styled(_("Save"), &ROLE_PRIMARY, font->fonts, (SDL_Keycode)0);
  ButtonWidget_t *btn_defaults =
      button_widget_create_styled(_("Load defaults"), &ROLE_PRIMARY, font->fonts, (SDL_Keycode)0);
  if (btn_save) {
    btn_save->base.rect.x = panel_x;
    btn_save->base.rect.y = buttons_y;
    btn_save->interactive = false;
    ui_register(&reg, &btn_save->base);
  }
  if (btn_defaults) {
    btn_defaults->base.rect.x =
        (btn_save ? btn_save->base.rect.x + btn_save->base.rect.w + g_layout_cfg.settings_save_btn_gap
                  : panel_x);
    btn_defaults->base.rect.y = buttons_y;
    ui_register(&reg, &btn_defaults->base);
  }

  TextWidget_t *tw_page = text_widget_create("", font->fonts[FONT_DEFAULT], DC_TEXT_ON_DARK);
  if (tw_page)
    ui_register(&reg, &tw_page->base);

  /* Hint above the table (registered so it auto-renders/destroys). */
  TextWidget_t *tw_hint = text_widget_create(_("Click an action, then press a key"),
                                             font->fonts[FONT_DEFAULT], DC_TEXT_ON_DARK);
  if (tw_hint) {
    ui_widget_place(&tw_hint->base, panel_x, panel_top - 44);
    ui_register(&reg, &tw_hint->base);
  }

  /* Title: black on a translucent orange pill, matching the other menus. */
  TextWidget_t *tw_title =
      text_widget_create(_("Hotkeys"), font->fonts[FONT_TITLE], get_color(COLOR_BLACK));
  if (tw_title)
    ui_widget_place(&tw_title->base, g_layout.menu.title_x, g_layout.menu.title_y);

  int capturing = -1; /* index into rows[] being bound, or -1 */
  bool running = true;
  Uint32 anim_start = SDL_GetTicks();

  while (running) {
    int page_start = page * rows_per_page;
    int page_end =
        (page_start + rows_per_page < (int)n_rows) ? page_start + rows_per_page : (int)n_rows;

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      SDL_Point mouse_pos = {e.button.x, e.button.y};
      if (btn_save)
        btn_save->base.hovered = SDL_PointInRect(&mouse_pos, &btn_save->base.rect);
      if (btn_defaults)
        btn_defaults->base.hovered = SDL_PointInRect(&mouse_pos, &btn_defaults->base.rect);
      if (btn_prev)
        btn_prev->base.hovered = SDL_PointInRect(&mouse_pos, &btn_prev->base.rect);
      if (btn_next)
        btn_next->base.hovered = SDL_PointInRect(&mouse_pos, &btn_next->base.rect);
      if (back_img)
        back_img->base.hovered = SDL_PointInRect(&mouse_pos, &back_img->base.rect);

      if (e.type == SDL_QUIT) {
        SDL_PushEvent(&e);
        running = false;
      } else if (capturing >= 0 && e.type == SDL_KEYDOWN) {
        SDL_Keycode sym = e.key.keysym.sym;
        if (sym == SDLK_ESCAPE) {
          if (rows[capturing].value)
            text_widget_set_text(rows[capturing].value, rows[capturing].keyname);
          capturing = -1;
        } else if (is_reserved_hotkey(sym)) {
          if (tw_hint)
            text_widget_set_text(tw_hint, _("That key is reserved - pick another"));
        } else {
          const char *kn = SDL_GetKeyName(sym);
          /* A key SDL can't name (some non-Latin layouts) is ignored so the
           * action keeps its previous, still-resolvable binding. */
          char cand[SIZEOF_HOTKEY_NAME];
          cand[0] = '\0';
          if (kn && *kn) {
            snprintf(cand, sizeof(cand), "%s", kn);
            /* SDL names single keys uppercase ("C"); store lowercase to match
             * the lowercase defaults. Multi-char names (Space) untouched. */
            if (cand[1] == '\0')
              cand[0] = (char)tolower((unsigned char)cand[0]);
          }
          /* Reject a key already bound to a *conflicting* action (group-aware:
           * check/call and raise/complete may share a key — see hotkeys_conflict). */
          const char *cap_action = player_config_entries[rows[capturing].cfg_idx].key + 7;
          int clash = -1;
          for (size_t r = 0; cand[0] && r < n_rows; r++) {
            if ((int)r == capturing || strcmp(rows[r].keyname, cand) != 0)
              continue;
            const char *r_action = player_config_entries[rows[r].cfg_idx].key + 7;
            if (hotkeys_conflict(cap_action, r_action)) {
              clash = (int)r;
              break;
            }
          }
          if (clash >= 0) {
            if (tw_hint)
              text_widget_set_text(tw_hint, _("That key is already used - pick another"));
          } else {
            if (cand[0])
              snprintf(rows[capturing].keyname, sizeof(rows[capturing].keyname), "%s", cand);
            if (rows[capturing].value)
              text_widget_set_text(rows[capturing].value, rows[capturing].keyname);
            capturing = -1;
          }
        }
      } else if (e.type == SDL_KEYDOWN) {
        if (e.key.keysym.sym == SDLK_ESCAPE)
          running = false;
      } else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
        if (btn_save && btn_save->interactive &&
            SDL_PointInRect(&mouse_pos, &btn_save->base.rect)) {
          btn_save->click.start_time = SDL_GetTicks();
          for (size_t r = 0; r < n_rows; r++)
            player_config_set_field(player_config, rows[r].cfg_idx, rows[r].keyname);
          save_player_config(player_config);
          init_hotkeys(player_config);
          running = false;
        } else if (btn_defaults && SDL_PointInRect(&mouse_pos, &btn_defaults->base.rect)) {
          btn_defaults->click.start_time = SDL_GetTicks();
          capturing = -1;
          for (size_t r = 0; r < n_rows; r++) {
            const char *def = hotkey_default_for_key(player_config_entries[rows[r].cfg_idx].key);
            if (def)
              snprintf(rows[r].keyname, sizeof(rows[r].keyname), "%s", def);
            if (rows[r].value)
              text_widget_set_text(rows[r].value, rows[r].keyname);
            player_config_set_field(player_config, rows[r].cfg_idx, rows[r].keyname);
          }
          save_player_config(player_config);
          init_hotkeys(player_config);
          if (tw_hint)
            text_widget_set_text(tw_hint, _("Defaults restored"));
        } else if (back_img && SDL_PointInRect(&mouse_pos, &back_img->base.rect)) {
          running = false;
        } else if (btn_prev && SDL_PointInRect(&mouse_pos, &btn_prev->base.rect) && page > 0) {
          btn_prev->click.start_time = SDL_GetTicks();
          page--;
          capturing = -1;
        } else if (btn_next && SDL_PointInRect(&mouse_pos, &btn_next->base.rect) &&
                   page < n_pages - 1) {
          btn_next->click.start_time = SDL_GetTicks();
          page++;
          capturing = -1;
        } else {
          for (int r = page_start; r < page_end; r++) {
            SDL_Rect b = {box_x, panel_top + (r - page_start) * row_h + (row_h - box_h) / 2, box_w,
                          box_h};
            if (SDL_PointInRect(&mouse_pos, &b)) {
              capturing = r;
              if (rows[r].value)
                text_widget_set_text(rows[r].value, _("press a key..."));
              if (tw_hint)
                text_widget_set_text(tw_hint, _("Click an action, then press a key"));
              break;
            }
          }
        }
      }
    }

    page_start = page * rows_per_page;
    page_end =
        (page_start + rows_per_page < (int)n_rows) ? page_start + rows_per_page : (int)n_rows;

    if (btn_save) {
      bool dirty = false;
      for (size_t r = 0; r < n_rows && !dirty; r++) {
        const char *field =
            (const char *)((const uint8_t *)player_config + player_config_entries[rows[r].cfg_idx].offset);
        if (strcmp(rows[r].keyname, field) != 0)
          dirty = true;
      }
      btn_save->interactive = dirty;
    }

    /* Position + enable current-page rows; hide the rest. Value text is centered
       in its key-box (drawn below). */
    for (int r = 0; r < (int)n_rows; r++) {
      bool on = (r >= page_start && r < page_end);
      int ry = panel_top + (r - page_start) * row_h;
      if (rows[r].label) {
        rows[r].label->base.enabled = on;
        if (on) {
          rows[r].label->base.rect.x = label_x;
          rows[r].label->base.rect.y = ry + (row_h - rows[r].label->base.rect.h) / 2;
        }
      }
      if (rows[r].value) {
        rows[r].value->base.enabled = on;
        if (on) {
          rows[r].value->base.rect.x = box_x + (box_w - rows[r].value->base.rect.w) / 2;
          rows[r].value->base.rect.y = ry + (row_h - rows[r].value->base.rect.h) / 2;
        }
      }
    }

    if (btn_prev)
      btn_prev->base.enabled = (n_pages > 1);
    if (btn_next)
      btn_next->base.enabled = (n_pages > 1);
    if (tw_page) {
      tw_page->base.enabled = (n_pages > 1);
      char pbuf[32];
      snprintf(pbuf, sizeof(pbuf), "Page %d / %d", page + 1, n_pages);
      text_widget_set_text(tw_page, pbuf);
      tw_page->base.rect.x = panel_x + (panel_w - tw_page->base.rect.w) / 2;
      tw_page->base.rect.y =
          pager_y + (btn_prev ? (btn_prev->base.rect.h - tw_page->base.rect.h) / 2 : 0);
    }

    if (back_img) {
      float t = (SDL_GetTicks() - anim_start) / 1000.0f;
      if (t > 1.0f)
        t = 1.0f;
      int start_y = g_viewport.y + g_viewport.h * 2 / 3;
      int end_y = g_viewport.y + g_viewport.h - g_layout_cfg.back_btn_size - 20;
      back_img->base.rect.y = start_y + (int)(t * (end_y - start_y));
    }

    clear_screen(sdl_context->renderer);

    /* Dark table panel. */
    draw_round_rect(sdl_context->renderer,
                    (SDL_Rect){panel_x - 16, panel_top - 16, panel_w + 32, panel_h + 32}, 24,
                    (SDL_Color){0, 0, 0, 150});

    if (tw_title) {
      SDL_Color title_pill = get_color(COLOR_ORANGE);
      title_pill.a = 64;
      draw_pill_behind(sdl_context->renderer, tw_title->base.rect, 22, 8, title_pill);
      ui_widget_render(&tw_title->base);
    }

    /* Key-boxes for the on-page rows (filled dark, bordered; gold while capturing). */
    SDL_Point mp;
    SDL_GetMouseState(&mp.x, &mp.y);
    for (int r = page_start; r < page_end; r++) {
      SDL_Rect b = {box_x, panel_top + (r - page_start) * row_h + (row_h - box_h) / 2, box_w, box_h};
      bool active = (capturing == r);
      bool hover = SDL_PointInRect(&mp, &b);
      SDL_SetRenderDrawColor(sdl_context->renderer, 25, 25, 25, 255);
      SDL_RenderFillRect(sdl_context->renderer, &b);
      if (active)
        SDL_SetRenderDrawColor(sdl_context->renderer, 240, 204, 48, 255);
      else if (hover)
        SDL_SetRenderDrawColor(sdl_context->renderer, 200, 200, 200, 255);
      else
        SDL_SetRenderDrawColor(sdl_context->renderer, 110, 110, 110, 255);
      SDL_RenderDrawRect(sdl_context->renderer, &b);
    }

    ui_render_all(&reg);
    SDL_RenderPresent(sdl_context->renderer);
    SDL_Delay(16);
  }

  if (tw_title)
    ui_widget_destroy(&tw_title->base);
  ui_destroy_all(&reg);
}

void menu_display_settings(PlayerConfig_t *player_config, SdlContext_t *sdl_context,
                                  Font_t *font, const Path_t *path) {
  const int input_w = g_layout_cfg.settings_input_w;

  UIRegistry_t reg = {0};

  /* Back arrow (top-left). */
  char *back_img_path = canfigger_path_join(path->data, "images/arrow_back.png");
  ImageWidget_t *back_img = back_img_path ? image_widget_create(back_img_path,
                                                                g_layout_cfg.back_btn_size,
                                                                g_layout_cfg.back_btn_size)
                                          : NULL;
  free(back_img_path);
  if (back_img) {
    back_img->base.rect.x = g_layout.menu.back_img_x;
    back_img->base.rect.y = g_layout.menu.back_img_y;
    ui_register(&reg, &back_img->base);
  }

  /* One row per editable config entry. host/port (startup screen) and the
     hotkey_* entries (their own sub-screen) are skipped. Bool entries get a
     checkbox; everything else a text input (password masked). Driven entirely by
     player_config_entries[] so the screen scales as entries are added (#76). */
  typedef struct {
    size_t cfg_idx;
    TextWidget_t *label;
    InputWidget_t *input;       /* NULL for bool/action rows */
    CheckboxWidget_t *checkbox; /* NULL for non-bool rows */
    ButtonWidget_t *button;     /* action rows (e.g. Hotkeys); not a config value */
    char init_str[MAX_INPUT_LENGTH];
    bool init_checked;
  } SettingsRow_t;

  SettingsRow_t rows[MAX_PLAYER_CONFIG_ENTRIES];
  size_t n_rows = 0;

  int checkbox_size;
  TTF_SizeUTF8(font->fonts[FONT_DEFAULT], "Ag", NULL, &checkbox_size);
  checkbox_size += g_layout_cfg.checkbox_pad;

  for (size_t i = 0; i < player_config_entry_count; i++) {
    /* nick(0) is edited on the connect screen; host(1)/port(2) on startup. */
    if (is_hotkey_entry(i) || i == 0 || i == 1 || i == 2)
      continue;
    SettingsRow_t *row = &rows[n_rows];
    row->cfg_idx = i;
    row->input = NULL;
    row->checkbox = NULL;
    row->button = NULL;
    row->init_str[0] = '\0';
    row->init_checked = false;

    row->label = text_widget_create(player_config_entries[i].key, font->fonts[FONT_DEFAULT],
                                    DC_TEXT_ON_DARK);
    if (row->label)
      ui_register(&reg, &row->label->base);

    const void *field = (const uint8_t *)player_config + player_config_entries[i].offset;
    if (player_config_entries[i].type == CFG_TYPE_BOOL) {
      row->init_checked = *(const bool *)field;
      row->checkbox = checkbox_widget_create(row->init_checked, checkbox_size);
      if (row->checkbox)
        ui_register(&reg, &row->checkbox->base);
    } else {
      switch (player_config_entries[i].type) {
      case CFG_TYPE_INT:
        snprintf(row->init_str, sizeof(row->init_str), "%d", *(const int *)field);
        break;
      case CFG_TYPE_UINT8:
        snprintf(row->init_str, sizeof(row->init_str), "%u", (unsigned)*(const uint8_t *)field);
        break;
      case CFG_TYPE_UINT16:
        snprintf(row->init_str, sizeof(row->init_str), "%u", (unsigned)*(const uint16_t *)field);
        break;
      case CFG_TYPE_UINT32:
        snprintf(row->init_str, sizeof(row->init_str), "%u", (unsigned)*(const uint32_t *)field);
        break;
      default: /* CFG_TYPE_STRING */
        snprintf(row->init_str, sizeof(row->init_str), "%s", (const char *)field);
        break;
      }
      row->input = input_widget_create(row->init_str, font->fonts[FONT_DEFAULT], input_w,
                                       player_config_entries[i].type);
      if (!row->input) {
        ui_destroy_all(&reg);
        return;
      }
      if (player_config_entries[i].type == CFG_TYPE_STRING && player_config_entries[i].size > 1)
        row->input->max_len = player_config_entries[i].size - 1;
      if (player_config_entries[i].type == CFG_TYPE_INT)
        row->input->max_val = 10;
      if (strcmp(player_config_entries[i].key, "password") == 0)
        row->input->masked = true;
      ui_register(&reg, &row->input->base);
    }
    n_rows++;
  }

  /* Action row: opens the Hotkeys sub-screen. Not a config value, so it's skipped
     by the dirty/save/defaults logic (which only touches input/checkbox rows). */
  {
    SettingsRow_t *row = &rows[n_rows];
    row->cfg_idx = (size_t)-1;
    row->input = NULL;
    row->checkbox = NULL;
    row->init_str[0] = '\0';
    row->init_checked = false;
    row->label = text_widget_create("hotkeys", font->fonts[FONT_DEFAULT], DC_TEXT_ON_DARK);
    if (row->label)
      ui_register(&reg, &row->label->base);
    row->button = button_widget_create_styled(_("Edit"), &ROLE_PRIMARY, font->fonts, (SDL_Keycode)0);
    if (row->button)
      ui_register(&reg, &row->button->base);
    n_rows++;
  }

  /* Paginate the rows. The page size is deliberately larger than the current
     entry count, so today everything fits on one page and the pager stays
     hidden; the pagination code is here for when more settings are added (#76). */
  const int rows_per_page = 8;
  const int n_pages =
      n_rows ? (int)((n_rows + (size_t)rows_per_page - 1) / (size_t)rows_per_page) : 1;
  int page = 0;

  /* Table geometry (a dark panel; white labels left, controls right). The panel
     is sized to the rows on a full page (or fewer if that's all there is) so a
     single short page has no empty space. */
  const int rows_shown = (n_rows < (size_t)rows_per_page) ? (int)n_rows : rows_per_page;
  const int panel_w = 1000;
  const int panel_x = g_viewport.x + (g_viewport.w - panel_w) / 2;
  const int panel_top = g_viewport.y + 180;
  const int row_h = 64;
  const int panel_h = (rows_shown > 0 ? rows_shown : 1) * row_h;
  const int label_x = panel_x + 40;
  const int control_x = panel_x + panel_w - input_w - 60;
  const int pager_y = panel_top + panel_h + 24;
  const int buttons_y = pager_y + 64;

  /* Pagination controls. */
  ButtonWidget_t *btn_prev =
      button_widget_create_styled(_("Prev"), &ROLE_PRIMARY, font->fonts, (SDL_Keycode)0);
  ButtonWidget_t *btn_next =
      button_widget_create_styled(_("Next"), &ROLE_PRIMARY, font->fonts, (SDL_Keycode)0);
  if (btn_prev) {
    btn_prev->base.rect.x = panel_x;
    btn_prev->base.rect.y = pager_y;
    ui_register(&reg, &btn_prev->base);
  }
  if (btn_next) {
    btn_next->base.rect.x = panel_x + panel_w - btn_next->base.rect.w;
    btn_next->base.rect.y = pager_y;
    ui_register(&reg, &btn_next->base);
  }

  /* Register each button immediately after its own NULL-check: if the second
     create fails, the first is already owned by `reg` and freed by the
     ui_destroy_all below, rather than leaking. */
  ButtonWidget_t *btn_save =
      button_widget_create_styled(_("Save"), &ROLE_PRIMARY, font->fonts, (SDL_Keycode)0);
  if (!btn_save) {
    ui_destroy_all(&reg);
    return;
  }
  btn_save->base.rect.x = panel_x;
  btn_save->base.rect.y = buttons_y;
  btn_save->interactive = false;
  ui_register(&reg, &btn_save->base);
  ButtonWidget_t *btn_defaults =
      button_widget_create_styled(_("Load Defaults"), &ROLE_PRIMARY, font->fonts, (SDL_Keycode)0);
  if (!btn_defaults) {
    ui_destroy_all(&reg);
    return;
  }
  btn_defaults->base.rect.x =
      btn_save->base.rect.x + btn_save->base.rect.w + g_layout_cfg.settings_save_btn_gap;
  btn_defaults->base.rect.y = buttons_y;
  ui_register(&reg, &btn_defaults->base);

  ButtonWidget_t *btn_quit_settings =
      button_widget_create_styled("X", &ROLE_DANGER, font->fonts, (SDL_Keycode)0);
  if (btn_quit_settings) {
    btn_quit_settings->base.rect.x =
        g_viewport.x + g_viewport.w - btn_quit_settings->base.rect.w - g_layout_cfg.margin;
    btn_quit_settings->base.rect.y = g_layout.menu.quit_y;
    ui_register(&reg, &btn_quit_settings->base);
  }

  /* Page indicator (positioned/updated each frame); registered last so it draws
     on top. */
  TextWidget_t *tw_page = text_widget_create("", font->fonts[FONT_DEFAULT], DC_TEXT_ON_DARK);
  if (tw_page)
    ui_register(&reg, &tw_page->base);

  /* Title: black text on a translucent orange pill (drawn in the loop), matching
     the connect screen so it reads on the felt. */
  TextWidget_t *tw_settings_title =
      text_widget_create(_("Settings"), font->fonts[FONT_TITLE], get_color(COLOR_BLACK));
  if (tw_settings_title)
    ui_widget_place(&tw_settings_title->base, g_layout.menu.title_x, g_layout.menu.title_y);

  /* Focus the first text input on the current page. */
  int focused_row = -1;
  for (int r = page * rows_per_page; r < (int)n_rows && r < (page + 1) * rows_per_page; r++)
    if (rows[r].input) {
      focused_row = r;
      rows[r].input->focused = true;
      break;
    }

  Uint32 anim_start = SDL_GetTicks();
  SDL_StartTextInput();
  bool running = true;
  bool saved = false;
  bool dirty = false;

  while (running) {
    int page_start = page * rows_per_page;
    int page_end =
        (page_start + rows_per_page < (int)n_rows) ? page_start + rows_per_page : (int)n_rows;

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      SDL_Point mouse_pos = {e.button.x, e.button.y};
      btn_save->base.hovered = SDL_PointInRect(&mouse_pos, &btn_save->base.rect);
      btn_defaults->base.hovered = SDL_PointInRect(&mouse_pos, &btn_defaults->base.rect);
      if (btn_prev)
        btn_prev->base.hovered = SDL_PointInRect(&mouse_pos, &btn_prev->base.rect);
      if (btn_next)
        btn_next->base.hovered = SDL_PointInRect(&mouse_pos, &btn_next->base.rect);
      if (back_img)
        back_img->base.hovered = SDL_PointInRect(&mouse_pos, &back_img->base.rect);
      if (btn_quit_settings)
        btn_quit_settings->base.hovered =
            SDL_PointInRect(&mouse_pos, &btn_quit_settings->base.rect);
      for (int r = page_start; r < page_end; r++) {
        if (rows[r].checkbox)
          rows[r].checkbox->base.hovered = SDL_PointInRect(&mouse_pos, &rows[r].checkbox->base.rect);
        if (rows[r].button)
          rows[r].button->base.hovered = SDL_PointInRect(&mouse_pos, &rows[r].button->base.rect);
      }

      if (e.type == SDL_QUIT) {
        SDL_PushEvent(&e);
        running = false;
      } else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
        if (btn_quit_settings && SDL_PointInRect(&mouse_pos, &btn_quit_settings->base.rect) &&
            confirm_quit(font->fonts)) {
          SDL_Event quit = {.type = SDL_QUIT};
          SDL_PushEvent(&quit);
          running = false;
        } else if (btn_save->interactive && SDL_PointInRect(&mouse_pos, &btn_save->base.rect)) {
          btn_save->click.start_time = SDL_GetTicks();
          for (size_t r = 0; r < n_rows; r++) {
            if (rows[r].checkbox)
              player_config_set_field(player_config, rows[r].cfg_idx,
                                      rows[r].checkbox->checked ? "yes" : "no");
            else if (rows[r].input)
              player_config_set_field(player_config, rows[r].cfg_idx,
                                      input_widget_get_text(rows[r].input));
          }
          save_player_config(player_config);
          running = false;
        } else if (SDL_PointInRect(&mouse_pos, &btn_defaults->base.rect)) {
          btn_defaults->click.start_time = SDL_GetTicks();
          for (size_t r = 0; r < n_rows; r++) {
            if (!rows[r].input && !rows[r].checkbox)
              continue; /* action rows (e.g. Hotkeys) have no config value */
            if (rows[r].cfg_idx == 0)
              continue; /* keep the player's nick */
            const char *def = player_config_entries[rows[r].cfg_idx].default_value;
            if (rows[r].checkbox)
              rows[r].checkbox->checked = strcmp(def, "yes") == 0;
            else if (rows[r].input)
              input_widget_set_text(rows[r].input, def);
          }
        } else if (back_img && SDL_PointInRect(&mouse_pos, &back_img->base.rect)) {
          running = false;
        } else if (btn_prev && SDL_PointInRect(&mouse_pos, &btn_prev->base.rect) && page > 0) {
          btn_prev->click.start_time = SDL_GetTicks();
          if (focused_row >= 0 && rows[focused_row].input)
            rows[focused_row].input->focused = false;
          page--;
          focused_row = -1;
          for (int r = page * rows_per_page; r < (int)n_rows && r < (page + 1) * rows_per_page; r++)
            if (rows[r].input) {
              focused_row = r;
              rows[r].input->focused = true;
              break;
            }
        } else if (btn_next && SDL_PointInRect(&mouse_pos, &btn_next->base.rect) &&
                   page < n_pages - 1) {
          btn_next->click.start_time = SDL_GetTicks();
          if (focused_row >= 0 && rows[focused_row].input)
            rows[focused_row].input->focused = false;
          page++;
          focused_row = -1;
          for (int r = page * rows_per_page; r < (int)n_rows && r < (page + 1) * rows_per_page; r++)
            if (rows[r].input) {
              focused_row = r;
              rows[r].input->focused = true;
              break;
            }
        } else {
          for (int r = page_start; r < page_end; r++) {
            if (rows[r].checkbox && SDL_PointInRect(&mouse_pos, &rows[r].checkbox->base.rect)) {
              rows[r].checkbox->checked = !rows[r].checkbox->checked;
              break;
            }
            if (rows[r].input && SDL_PointInRect(&mouse_pos, &rows[r].input->base.rect)) {
              if (focused_row >= 0 && rows[focused_row].input)
                rows[focused_row].input->focused = false;
              focused_row = r;
              rows[r].input->focused = true;
              break;
            }
            if (rows[r].button && SDL_PointInRect(&mouse_pos, &rows[r].button->base.rect)) {
              rows[r].button->click.start_time = SDL_GetTicks();
              menu_display_hotkeys(player_config, sdl_context, font, path);
              break;
            }
          }
        }
      } else if (e.type == SDL_TEXTINPUT) {
        if (focused_row >= 0 && rows[focused_row].input)
          input_widget_append(rows[focused_row].input, e.text.text);
      } else if (e.type == SDL_KEYDOWN) {
        InputWidget_t *fin = (focused_row >= 0) ? rows[focused_row].input : NULL;
        switch (e.key.keysym.sym) {
        case SDLK_RETURN:
          saved = true;
          running = false;
          break;
        case SDLK_ESCAPE:
          running = false;
          break;
        case SDLK_TAB: {
          /* Cycle focus through the text inputs on the current page. */
          int order[MAX_PLAYER_CONFIG_ENTRIES];
          int n_ord = 0;
          for (int r = page_start; r < page_end; r++)
            if (rows[r].input)
              order[n_ord++] = r;
          if (n_ord == 0)
            break;
          int cur = 0;
          for (int k = 0; k < n_ord; k++)
            if (order[k] == focused_row)
              cur = k;
          if (fin)
            fin->focused = false;
          int dir = (e.key.keysym.mod & KMOD_SHIFT) ? -1 : 1;
          cur = (cur + dir + n_ord) % n_ord;
          focused_row = order[cur];
          /* order[] only holds collected row indices (all >= 0 with a non-NULL
             input), so this is in-bounds — guard anyway to match every other
             rows[focused_row] access in this function. */
          if (focused_row >= 0 && rows[focused_row].input)
            rows[focused_row].input->focused = true;
          break;
        }
        case SDLK_BACKSPACE:
          if (fin)
            input_widget_backspace(fin);
          break;
        case SDLK_LEFT:
          if (fin)
            input_widget_cursor_left(fin);
          break;
        case SDLK_RIGHT:
          if (fin)
            input_widget_cursor_right(fin);
          break;
        case SDLK_HOME:
          if (fin)
            input_widget_cursor_home(fin);
          break;
        case SDLK_END:
          if (fin)
            input_widget_cursor_end(fin);
          break;
        case SDLK_v:
          if ((e.key.keysym.mod & KMOD_CTRL) && fin) {
            char *clip = SDL_GetClipboardText();
            if (clip) {
              input_widget_append(fin, clip);
              SDL_free(clip);
            }
          }
          break;
        case SDLK_F11:
          toggle_fullscreen(sdl_context);
          break;
        default:
          break;
        }
      }
    }

    /* page may have changed above; recompute the visible range. */
    page_start = page * rows_per_page;
    page_end =
        (page_start + rows_per_page < (int)n_rows) ? page_start + rows_per_page : (int)n_rows;

    dirty = false;
    for (size_t r = 0; r < n_rows && !dirty; r++) {
      if (rows[r].checkbox && rows[r].checkbox->checked != rows[r].init_checked)
        dirty = true;
      else if (rows[r].input && strcmp(input_widget_get_text(rows[r].input), rows[r].init_str) != 0)
        dirty = true;
    }
    btn_save->interactive = dirty;

    /* Position + enable the current page's widgets; hide the rest. */
    for (int r = 0; r < (int)n_rows; r++) {
      bool on = (r >= page_start && r < page_end);
      int slot = r - page_start;
      if (rows[r].label) {
        rows[r].label->base.enabled = on;
        if (on) {
          rows[r].label->base.rect.x = label_x;
          rows[r].label->base.rect.y =
              panel_top + slot * row_h + (row_h - rows[r].label->base.rect.h) / 2;
        }
      }
      if (rows[r].input) {
        rows[r].input->base.enabled = on;
        if (on) {
          rows[r].input->base.rect.x = control_x;
          rows[r].input->base.rect.y =
              panel_top + slot * row_h + (row_h - rows[r].input->base.rect.h) / 2;
        }
      }
      if (rows[r].checkbox) {
        rows[r].checkbox->base.enabled = on;
        if (on) {
          rows[r].checkbox->base.rect.x = control_x;
          rows[r].checkbox->base.rect.y =
              panel_top + slot * row_h + (row_h - rows[r].checkbox->base.rect.h) / 2;
        }
      }
      if (rows[r].button) {
        rows[r].button->base.enabled = on;
        if (on) {
          rows[r].button->base.rect.x = control_x;
          rows[r].button->base.rect.y =
              panel_top + slot * row_h + (row_h - rows[r].button->base.rect.h) / 2;
        }
      }
    }

    /* Pager only matters with more than one page. */
    if (btn_prev)
      btn_prev->base.enabled = (n_pages > 1);
    if (btn_next)
      btn_next->base.enabled = (n_pages > 1);
    if (tw_page) {
      tw_page->base.enabled = (n_pages > 1);
      char pbuf[32];
      snprintf(pbuf, sizeof(pbuf), "Page %d / %d", page + 1, n_pages);
      text_widget_set_text(tw_page, pbuf);
      tw_page->base.rect.x = panel_x + (panel_w - tw_page->base.rect.w) / 2;
      tw_page->base.rect.y =
          pager_y + (btn_prev ? (btn_prev->base.rect.h - tw_page->base.rect.h) / 2 : 0);
    }

    if (back_img) {
      float t = (SDL_GetTicks() - anim_start) / 1000.0f;
      if (t > 1.0f)
        t = 1.0f;
      int start_y = g_viewport.y + g_viewport.h * 2 / 3;
      int end_y = g_viewport.y + g_viewport.h - g_layout_cfg.back_btn_size - 20;
      back_img->base.rect.y = start_y + (int)(t * (end_y - start_y));
    }

    clear_screen(sdl_context->renderer);

    /* Dark table panel behind the rows. */
    draw_round_rect(sdl_context->renderer,
                    (SDL_Rect){panel_x - 16, panel_top - 16, panel_w + 32, panel_h + 32}, 24,
                    (SDL_Color){0, 0, 0, 150});

    if (tw_settings_title) {
      SDL_Color title_pill = get_color(COLOR_ORANGE);
      title_pill.a = 64;
      draw_pill_behind(sdl_context->renderer, tw_settings_title->base.rect, 22, 8, title_pill);
      ui_widget_render(&tw_settings_title->base);
    }

    ui_render_all(&reg);

    SDL_RenderPresent(sdl_context->renderer);
    SDL_Delay(16);
  }

  SDL_StopTextInput();

  if (saved && dirty) {
    for (size_t r = 0; r < n_rows; r++) {
      if (rows[r].checkbox)
        player_config_set_field(player_config, rows[r].cfg_idx,
                                rows[r].checkbox->checked ? "yes" : "no");
      else if (rows[r].input)
        player_config_set_field(player_config, rows[r].cfg_idx,
                                input_widget_get_text(rows[r].input));
    }
    save_player_config(player_config);
  }

  if (tw_settings_title)
    ui_widget_destroy(&tw_settings_title->base);
  ui_destroy_all(&reg);
}
