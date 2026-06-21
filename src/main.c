/*
 main.c
 https://github.com/Dealer-s-Choice/dealers_choice

 MIT License

 Copyright (c) 2025 Andy Alt

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
#include <errno.h>
#include <stdio.h>
#include <stdlib.h> // For setenv()
#include <string.h>

#include "cli.h"
#include "client.h"
#include "config.h"
#include "dc_config.h"
#include "game.h"
#include "globals_gui.h"
#include "graphics.h"
#include "hotkeys.h"
#include "lan_discovery.h"
#include "links.h"
#include "main.h"
#include "menus.h"
#include "util.h"
#include "widgets/button.h"
#include "widgets/card.h"
#include "widgets/card_text_atlas.h"
#include "widgets/checkbox.h"
#include "widgets/image.h"
#include "widgets/input.h"
#include "widgets/text.h"
#include <sodium.h>

/* --card-preview mode: open the SDL window, render the A and 10 of each
 * suit in a 2x4 grid on the green felt, and wait for x / ESC / window
 * close.  Lets you iterate on suit size, position, and font without
 * spinning up a server and a bot for every visual check. */
static int run_card_preview(SdlContext_t *sdl_context, Font_t *font, Path_t *path) {
  char felt_path[4096];
  snprintf(felt_path, sizeof(felt_path), "%s/images/100x100-green-felt-seamless-tile.png",
           path->data);
  SDL_Texture *felt_tex = load_texture(sdl_context->renderer, felt_path);

  static const struct {
    int face;
    int suit;
  } TEST_CARDS[8] = {
      {DH_CARD_ACE, DH_SUIT_SPADES},   {DH_CARD_ACE, DH_SUIT_HEARTS},
      {DH_CARD_ACE, DH_SUIT_DIAMONDS}, {DH_CARD_ACE, DH_SUIT_CLUBS},
      {DH_CARD_TEN, DH_SUIT_SPADES},   {DH_CARD_TEN, DH_SUIT_HEARTS},
      {DH_CARD_TEN, DH_SUIT_DIAMONDS}, {DH_CARD_TEN, DH_SUIT_CLUBS},
  };

  const int gap = 24;
  const int cw_w = g_layout_cfg.card_w;
  const int cw_h = g_layout_cfg.card_h;
  const int grid_w = 4 * cw_w + 3 * gap;
  const int grid_h = 2 * cw_h + gap;
  const int origin_x = LOGICAL_WIDTH / 2 - grid_w / 2;
  const int origin_y = LOGICAL_HEIGHT / 2 - grid_h / 2;

  CardWidget_t cards[8];
  for (int i = 0; i < 8; i++) {
    int row = i / 4;
    int col = i % 4;
    card_widget_init(&cards[i], font->fonts[FONT_CARD]);
    cards[i].base.rect.x = origin_x + col * (cw_w + gap);
    cards[i].base.rect.y = origin_y + row * (cw_h + gap);
    cards[i].face_val = TEST_CARDS[i].face;
    cards[i].suit = TEST_CARDS[i].suit;
  }

  bool running = true;
  while (running) {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
      if (ev.type == SDL_QUIT)
        running = false;
      else if (ev.type == SDL_KEYDOWN &&
               (ev.key.keysym.sym == SDLK_x || ev.key.keysym.sym == SDLK_ESCAPE))
        running = false;
    }

    SDL_SetRenderDrawColor(sdl_context->renderer, 0, 100, 0, 255);
    SDL_RenderClear(sdl_context->renderer);
    if (felt_tex)
      draw_felt_background(sdl_context->renderer, felt_tex);

    for (int i = 0; i < 8; i++)
      cards[i].base.render(&cards[i].base);

    SDL_RenderPresent(sdl_context->renderer);
    SDL_Delay(16);
  }

  if (felt_tex)
    SDL_DestroyTexture(felt_tex);
  return 0;
}

static void init_sdl_window(SdlContext_t *c, const char *title) {
  SDL_Rect bounds;

  if (SDL_GetDisplayBounds(0, &bounds) != 0) {
    SDL_Log("SDL_GetDisplayBounds failed: %s", SDL_GetError());
    exit(EXIT_FAILURE);
  }

  const float factor = 0.8f;
  const int w = (int)(bounds.w * factor);
  const int h = (int)(bounds.h * factor);

  c->window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h,
                               SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI);

  if (!c->window) {
    SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
    exit(EXIT_FAILURE);
  }

  /* MUST be set BEFORE creating the renderer */
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0"); // nearest

  c->renderer =
      SDL_CreateRenderer(c->window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

  if (!c->renderer) {
    SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
    SDL_DestroyWindow(c->window);
    exit(EXIT_FAILURE);
  }

  if (SDL_RenderSetLogicalSize(c->renderer, LOGICAL_WIDTH, LOGICAL_HEIGHT) != 0) {
    SDL_Log("SDL_RenderSetLogicalSize failed: %s", SDL_GetError());
  }

  SDL_RenderGetViewport(c->renderer, &g_viewport);
  SDL_Log("Viewport: x=%d y=%d w=%d h=%d", g_viewport.x, g_viewport.y, g_viewport.w, g_viewport.h);

  g_center.x = g_viewport.x + g_viewport.w / 2;
  g_center.y = g_viewport.y + g_viewport.h / 2;
}

int main(int argc, char *argv[]) {
#ifdef ENABLE_NLS
  static char *locale_dir;
  locale_dir = getenv("DEALERSCHOICE_LOCALEDIR");
  if (!locale_dir)
    locale_dir = DEALERSCHOICE_LOCALEDIR;

  setlocale(LC_ALL, "");
  bindtextdomain(DEALERSCHOICE_NAME, locale_dir);
  textdomain(DEALERSCHOICE_NAME);
#endif
  Path_t path = {0};
  get_data_dir(&path);

  const CliArgs_t cli_args = parse_cli_args(argc, argv);

  if (SDL_Init(SDL_INIT_VIDEO) == -1) {
    fprintf(stderr, "SDL init failed: %s\n", SDL_GetError());
    return 1;
  }

  if (TTF_Init() == -1) {
    fprintf(stderr, "TTF_Init: %s\n", TTF_GetError());
    return -1;
  }

  SdlContext_t sdl_context = {0};
  init_sdl_window(&sdl_context, DEALERSCHOICE_FORMAL_NAME);
  g_sdl_context = &sdl_context;

  Font_t font;

  const FontArgs_t font_args[] = {
      [FONT_CARD] = {.file = "LiberationSans-Bold.ttf", .ptsize = 38},
      [FONT_DEFAULT] = {.file = "LiberationSans-Regular.ttf", .ptsize = 32},
      [FONT_DEFAULT_BOLD] = {.file = "LiberationSans-Bold.ttf", .ptsize = 32},
      [FONT_BOLD] = {.file = "LiberationSans-Bold.ttf", .ptsize = 26},
      [FONT_LINK] = {.file = "LiberationSans-Regular.ttf", .ptsize = 22},
      [FONT_STATUS_MSG] = {.file = "LiberationSans-Bold.ttf", .ptsize = 24},
      [FONT_TITLE] = {.file = "LiberationSerif-BoldItalic.ttf", .ptsize = 72},
      [FONT_VERSION] = {.file = "LiberationSans-Regular.ttf", .ptsize = 22},
      [FONT_WILD_SELECT] = {.file = "LiberationSans-Bold.ttf", .ptsize = 24},
  };

  for (int i = 0; i < NUM_FONTS; ++i) {
    char font_path[4096] = {0};
    snprintf(font_path, sizeof(font_path), "%s/%s", path.data, font_args[i].file);
    font.fonts[i] = open_font(&(FontArgs_t){font_path, font_args[i].ptsize});
    if (!font.fonts[i])
      return -1;
  }

  g_layout_cfg = get_layout_config(path.data);
  layout_init(TTF_FontHeight(font.fonts[FONT_STATUS_MSG]));

  PlayerConfig_t player_config = get_player_config();
  get_common_registries(path.data, player_config.registry_host, player_config.registry_port,
                        &player_config.registry_count, &player_config.lan_discovery_port);
  if (!player_config.loaded) {
    fprintf(stderr, "Unable to load config\n");
    exit(EXIT_FAILURE);
  }
  init_hotkeys(&player_config);

  /* Pre-render all 52 card text textures once.  card_widget_render
   * looks them up by (face_val, suit); otherwise it would call
   * TTF_RenderUTF8_Blended + SDL_CreateTextureFromSurface every frame
   * for every card (~6M allocations per 4 min of gameplay before this
   * cache landed). */
  card_text_atlas_init(sdl_context.renderer, font.fonts[FONT_CARD], path.data);

  if (cli_args.card_preview) {
    int rc = run_card_preview(&sdl_context, &font, &path);
    for (int i = 0; i < NUM_FONTS; ++i)
      TTF_CloseFont(font.fonts[i]);
    TTF_Quit();
    do_sdl_cleanup(&sdl_context);
    return rc;
  }

  /* Client networking + password hashing — only needed past this point. */
  if (sodium_init() < 0) {
    fprintf(stderr, "libsodium init failed\n");
    exit(1);
  }
  pcg_srand_auto();
  if (tcpme_init() != 0) {
    fprintf(stderr, "tcpme_init failed: %s\n", tcpme_get_error());
    return 1;
  }

#ifdef ENABLE_NLS
  if (strlen(player_config.language) != 0) {
#ifdef _WIN32
    _putenv_s("LANGUAGE", player_config.language);
#else
    setenv("LANGUAGE", player_config.language, 1);
#endif
    setlocale(LC_ALL, "");
    bindtextdomain(DEALERSCHOICE_NAME, locale_dir);
    textdomain(DEALERSCHOICE_NAME);
  }
#endif

  LinkWidget_t *links[LINK_DEFS_COUNT];
  for (size_t i = 0; i < LINK_DEFS_COUNT; i++)
    links[i] = link_widget_create(_(LINK_DEFS[i].text), LINK_DEFS[i].url, font.fonts[FONT_LINK]);
  layout_links(links, LINK_DEFS_COUNT);

  char host_str[MAX_INPUT_LENGTH] = {0};
  snprintf(host_str, sizeof(host_str), "%s", (cli_args.host) ? cli_args.host : player_config.host);

  uint16_t port = (cli_args.port != 0) ? cli_args.port : player_config.port;
  bool loop_to_connect = true;
  bool first_connect = true;
  while (loop_to_connect) {
    loop_to_connect = false;
    int connect_result;
    if (cli_args.auto_connect && first_connect) {
      connect_result = RUN_CLIENT;
    } else {
      do {
        connect_result =
            menu_display_connect(&player_config, host_str, &port, &sdl_context, &font, links);
        if (connect_result == RUN_SETTINGS)
          menu_display_settings(&player_config, &sdl_context, &font, &path);
      } while (connect_result == RUN_SETTINGS);
    }

    if (connect_result == RUN_CLIENT) {
      bool went_back = get_socket_context_and_run_client(&player_config, &cli_args, host_str, port,
                                                         &sdl_context, &font, &path, links, NULL);
      if (went_back) {
        SDL_Event peek;
        SDL_PumpEvents();
        if (SDL_PeepEvents(&peek, 1, SDL_PEEKEVENT, SDL_QUIT, SDL_QUIT) <= 0) {
          loop_to_connect = true;
          first_connect = false;
        }
      }
    }
  }

  for (size_t i = 0; i < LINK_DEFS_COUNT; i++)
    if (links[i])
      ui_widget_destroy(&links[i]->base);
  for (int i = 0; i < NUM_FONTS; ++i)
    TTF_CloseFont(font.fonts[i]);
  TTF_Quit();
  tcpme_quit();
  do_sdl_cleanup(&sdl_context);

  return 0;
}
