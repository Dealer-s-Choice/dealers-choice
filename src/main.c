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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h> // For setenv()
#include <string.h>

#include "client.h"
#include "config.h"
#include "game.h"
#include "getlongopt.h"
#include "graphics.h"
#include "main.h"
#include "server.h"

enum { RUN_CLIENT = 20 };

static int menu_display_connect(PlayerConfig_t *player_config, char *host_str, const uint16_t port,
                                SdlContext_t *sdl_context, Font_t *font) {
  Button_t button_connect = {
      .text = _("Connect"),
      .renderer = sdl_context->renderer,
      .bg_color = get_color(COLOR_BLACK),
      .fg_color = get_color(COLOR_YELLOW),
      .rect = {100, 160, 0, 0},
      .font = font->fonts[FONT_BOLD],
      .enabled = true,
      .active = true,
  };

  if (TTF_SizeUTF8(button_connect.font, button_connect.text, &button_connect.rect.w,
                   &button_connect.rect.h) != 0)
    fprintf(stderr, "TTF_SizeUTF8 error: %s\n", TTF_GetError());
  button_connect.rect.w += SCALE_X(20);
  button_connect.rect.h += SCALE_Y(10);

  SDL_Rect input_box = (SDL_Rect){100, 220, 200, 40};

  // TODO: Create a 'input_box' struct similar to CardContext_t. It will
  // be used for inputs such as the host ip, nick, and port
  // This isn't actually an input yet.
  SDL_Rect input_nick = (SDL_Rect){100, 380, 300, 40};
  SDL_StartTextInput();

  bool run_client = false;
  bool running = true;
  while (running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      SDL_Point mouse_pos = {e.button.x, e.button.y};
      button_connect.hovered = SDL_PointInRect(&mouse_pos, &button_connect.rect);
      if (e.type == SDL_QUIT) {
        running = false;
      } else if (e.type == SDL_MOUSEBUTTONDOWN) {
        if (SDL_PointInRect(&mouse_pos, &button_connect.rect)) {
          run_client = true;
          running = false;
        }
      } else if (e.type == SDL_TEXTINPUT) {
        if (strlen(host_str) + strlen(e.text.text) < MAX_INPUT_LENGTH) {
          strcat(host_str, e.text.text);
        }
      } else if (e.type == SDL_KEYDOWN) {
        if (e.key.keysym.sym == SDLK_BACKSPACE && strlen(host_str) > 0) {
          host_str[strlen(host_str) - 1] = '\0';
        } else if (e.key.keysym.sym == SDLK_RETURN && e.key.keysym.mod & KMOD_ALT) {
          toggle_fullscreen(sdl_context->window);
        } else if (e.key.keysym.sym == SDLK_RETURN) {
          run_client = true;
          running = false;
        }
      }
    }

    clear_screen(sdl_context->renderer);
    render_button(&button_connect);

    SDL_SetRenderDrawColor(sdl_context->renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(sdl_context->renderer, &input_box);
    SDL_Rect input_text_pos = {input_box.x, input_box.y, 0, 0};
    // TTF_SetFontSize
    // TTF_SetFontStyle(font->fonts[FONT_BOLD], TTF_STYLE_BOLD);
    render_text(sdl_context->renderer, font->fonts[FONT_DEFAULT], host_str, get_color(COLOR_WHITE),
                &input_text_pos);

    char port_str[24] = {0};
    snprintf(port_str, sizeof(port_str), _("port: %d"), port);
    render_text_plain(sdl_context->renderer, font->fonts[FONT_DEFAULT], port_str,
                      get_color(COLOR_BLACK),
                      &(SDL_Rect){input_box.x, input_box.y + SCALE_Y(50), 0, 0});

    SDL_Rect input_nick_pos = {input_nick.x, input_nick.y, 0, 0};
    render_text_plain(sdl_context->renderer, font->fonts[FONT_DEFAULT], player_config->nick,
                      get_color(COLOR_BLACK), &input_nick_pos);

    SDL_Rect title_rect = {sdl_context->win_center.x / 1.5, SCALE_Y(60), 0, 0};
    render_text_plain(sdl_context->renderer, font->fonts[FONT_TITLE], DEALERSCHOICE_FORMAL_NAME,
                      get_color(COLOR_BLACK), &title_rect);

    char version[64] = {0};
    snprintf(version, sizeof(version), "Version " DEALERSCHOICE_VERSION);
    render_text_plain(sdl_context->renderer, font->fonts[FONT_VERSION], version,
                      get_color(COLOR_WHITE),
                      &(SDL_Rect){title_rect.x + SCALE_X(40), title_rect.y + SCALE_Y(80), 0, 0});

    SDL_RenderPresent(sdl_context->renderer);
    SDL_Delay(16);
  }

  SDL_StopTextInput();
  return run_client == true ? RUN_CLIENT : 0;
}

static void print_version(void) {
  fputs(DEALERSCHOICE_FORMAL_NAME " v" DEALERSCHOICE_VERSION "\n", stdout);
  fputs(DEALERSCHOICE_URL "\n", stdout);
  putchar('\n');
}

static CliArgs_t parse_cli_args(int argc, char *argv[]) {
  enum {
    OPT_SERVER = 1,
    OPT_SERVER_LOG_GAME_RESULTS,
    OPT_SERVER_CONF,
    OPT_TEST,
    OPT_BIND,
    OPT_HOST,
    OPT_PORT,
    OPT_VERSION,
    OPT_VERBOSE,
    OPT_DISABLE_AUDIO,
  };

  static const glopt_option_t options[] = {
      {"server", GLOPT_NO_ARG, OPT_SERVER},
      {"server-log-game-results", GLOPT_REQUIRED_ARG, OPT_SERVER_LOG_GAME_RESULTS},
      {"server-conf", GLOPT_REQUIRED_ARG, OPT_SERVER_CONF},
      {"-test", GLOPT_NO_ARG, OPT_TEST},
      {"bind-address", GLOPT_REQUIRED_ARG, OPT_BIND},
      {"host", GLOPT_REQUIRED_ARG, OPT_HOST},
      {"port", GLOPT_REQUIRED_ARG, OPT_PORT},
      {"version", GLOPT_NO_ARG, OPT_VERSION},
      {"verbose", GLOPT_NO_ARG, OPT_VERBOSE},
      {"disable-audio", GLOPT_NO_ARG, OPT_DISABLE_AUDIO},
      {NULL, 0, 0}};

  glopt_parser_t parser;
  glopt_init(&parser, options);
  CliArgs_t cli_args = {0};

  int opt;
  while ((opt = glopt_next(&parser, argc, argv)) != -1) {
    switch (opt) {
    case OPT_SERVER:
      cli_args.run_server_flag = true;
      break;
    case OPT_SERVER_LOG_GAME_RESULTS:
      cli_args.server_log_game_results_file = parser.optarg;
      break;
    case OPT_SERVER_CONF:
      cli_args.server_conf = parser.optarg;
      break;
    case OPT_TEST:
      cli_args.test_mode = true;
      break;
    case OPT_BIND:
      cli_args.bind_address = parser.optarg;
      break;
    case OPT_HOST:
      cli_args.host = parser.optarg;
      break;
    case OPT_PORT: {
      unsigned long port_val;
      parse_unsigned(parser.optarg, UINT16_MAX, &port_val);
      cli_args.port = (uint16_t)port_val;
      break;
    }
    case OPT_VERSION:
      print_version();
      exit(EXIT_SUCCESS);
      break;
    case OPT_VERBOSE:
      verbose = true;
      break;
    case OPT_DISABLE_AUDIO:
      cli_args.disable_audio = true;
      break;
    case '?':
    default:
      print_version();
      fputs("Usage:\n"
            "  --verbose\n"
            "  --server [--bind-address IP]\n"
            "  --server-log-game-results [path/to/file]\n"
            "  --server-conf [Path to alternate server config file]\n"
            "  --host [IP]\n"
            "  --port [port]\n"
            "  --disable-audio\n"
            "  --version\n",
            stderr);
      exit(EXIT_FAILURE);
    }
  }
  return cli_args;
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

  pcg_srand_auto();

  if (cli_args.run_server_flag) {
    return run_server(&cli_args, &path);
  }

  if (SDL_Init(SDL_INIT_VIDEO) == -1 || SDLNet_Init() == -1) {
    fprintf(stderr, "SDL or SDL_net init failed: %s\n", SDLNet_GetError());
    return 1;
  }

  if (TTF_Init() == -1) {
    fprintf(stderr, "TTF_Init: %s\n", TTF_GetError());
    return -1;
  }

  SdlContext_t sdl_context;
  init_sdl_window(&sdl_context, DEALERSCHOICE_FORMAL_NAME);

  Font_t font;

  const FontArgs_t font_args[] = {
      [FONT_CARD] = {.file = "LiberationSans-Bold.ttf", .ptsize = 38},
      [FONT_DEFAULT] = {.file = "LiberationSans-Regular.ttf", .ptsize = 32},
      [FONT_DEFAULT_BOLD] = {.file = "LiberationSans-Bold.ttf", .ptsize = 32},
      [FONT_BOLD] = {.file = "LiberationSans-Bold.ttf", .ptsize = 26},
      [FONT_LINK] = {.file = "LiberationSans-Regular.ttf", .ptsize = 22},
      [FONT_STATUS_MSG] = {.file = "LiberationSans-Regular.ttf", .ptsize = 24},
      [FONT_TITLE] = {.file = "LiberationSerif-BoldItalic.ttf", .ptsize = 72},
      [FONT_VERSION] = {.file = "LiberationSans-Regular.ttf", .ptsize = 22},
      [FONT_WILD_SELECT] = {.file = "LiberationSans-Bold.ttf", .ptsize = 24},
  };

  for (int i = 0; i < NUM_FONTS; ++i) {
    char font_path[4096] = {0};
    snprintf(font_path, sizeof(font_path), "%s/%s", path.data, font_args[i].file);
    font.fonts[i] = open_font(&(FontArgs_t){font_path, SCALE_Y(font_args[i].ptsize)});
    if (!font.fonts[i])
      return -1;
  }

  PlayerConfig_t player_config = get_player_config();
  if (!player_config.loaded) {
    fprintf(stderr, "Unable to load config\n");
    exit(EXIT_FAILURE);
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

  char host_str[MAX_INPUT_LENGTH] = {0};
  snprintf(host_str, sizeof(host_str), "%s", (cli_args.host) ? cli_args.host : player_config.host);

  uint16_t port = (cli_args.port != 0) ? cli_args.port : player_config.port;
  if (menu_display_connect(&player_config, host_str, port, &sdl_context, &font) == RUN_CLIENT) {
    char tmp[256] = {0};
    snprintf(tmp, sizeof(tmp), "Attempting to connect to: %s...", host_str);
    render_text_plain(sdl_context.renderer, font.fonts[FONT_DEFAULT], tmp, get_color(COLOR_WHITE),
                      &(SDL_Rect){SCALE_X(10), sdl_context.win_center.y, 0, 0});
    SDL_RenderPresent(sdl_context.renderer);

    get_socket_context_and_run_client(&player_config, &cli_args, host_str, port, &sdl_context,
                                      &font, &path, cli_args.test_mode);
  }

  for (int i = 0; i < NUM_FONTS; ++i)
    TTF_CloseFont(font.fonts[i]);
  TTF_Quit();
  do_sdl_cleanup(&sdl_context);

  return 0;
}
