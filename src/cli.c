/*
 cli.c
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

/* Command-line parsing shared by the GUI and headless-server binaries.
 * SDL-free; part of libdc_core. */

#include <stdio.h>
#include <stdlib.h>

#include "cli.h"
#include "config.h"
#include "getlongopt.h"
#include "util.h"

static void print_version(void) {
  fputs(DEALERSCHOICE_FORMAL_NAME " v" DEALERSCHOICE_VERSION "\n", stdout);
  fputs(DEALERSCHOICE_URL "\n", stdout);
  putchar('\n');
}

CliArgs_t parse_cli_args(int argc, char *argv[]) {
  enum {
    OPT_SERVER = 1,
    OPT_HOST,
    OPT_PORT,
    OPT_VERSION,
    OPT_VERBOSE,
    OPT_DEBUG,
    OPT_DISABLE_AUDIO,
    OPT_AUTO_CONNECT,
    OPT_CARD_PREVIEW,
    OPT_LOG_FILE,
  };

  static const glopt_option_t options[] = {{"server", GLOPT_NO_ARG, OPT_SERVER, 0},
                                           {"host", GLOPT_REQUIRED_ARG, OPT_HOST, 0},
                                           {"port", GLOPT_REQUIRED_ARG, OPT_PORT, 0},
                                           {"version", GLOPT_NO_ARG, OPT_VERSION, 0},
                                           {"verbose", GLOPT_NO_ARG, OPT_VERBOSE, 0},
                                           {"debug", GLOPT_NO_ARG, OPT_DEBUG, 0},
                                           {"disable-audio", GLOPT_NO_ARG, OPT_DISABLE_AUDIO, 0},
                                           {"auto-connect", GLOPT_NO_ARG, OPT_AUTO_CONNECT, 0},
                                           {"card-preview", GLOPT_NO_ARG, OPT_CARD_PREVIEW, 0},
                                           {"log-file", GLOPT_REQUIRED_ARG, OPT_LOG_FILE, 0},
                                           {NULL, 0, 0, 0}};

  glopt_parser_t parser;
  glopt_init(&parser, options);
  CliArgs_t cli_args = {0};

  /* Deterministic test mode (fixed deck, short timeouts) is enabled via the
   * DC_TEST environment variable, not a user-facing flag. */
  const char *dc_test_env = getenv("DC_TEST");
  dc_test_mode = (dc_test_env && strcmp(dc_test_env, "1") == 0);

  int opt;
  while ((opt = glopt_next(&parser, argc, argv)) != -1) {
    switch (opt) {
    case OPT_SERVER:
      /* The server has moved to its own binary; this is a deprecation stub. */
      fputs("The server now runs as a separate program.\n"
            "Use '" DEALERSCHOICE_NAME "-server' instead (run it with --help for options).\n",
            stderr);
      exit(EXIT_FAILURE);
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
    case OPT_DEBUG:
      verbose = true;
      dc_debug = true;
      break;
    case OPT_DISABLE_AUDIO:
      cli_args.disable_audio = true;
      break;
    case OPT_AUTO_CONNECT:
      cli_args.auto_connect = true;
      break;
    case OPT_CARD_PREVIEW:
      cli_args.card_preview = true;
      break;
    case OPT_LOG_FILE:
      dc_log_set_file(parser.optarg);
      break;
    case '?':
    default:
      print_version();
      fputs(
          "Usage: " DEALERSCHOICE_NAME " [options]   (run a server with '" DEALERSCHOICE_NAME
          "-server')\n"
          "  --host [IP]\n"
          "  --port [port]\n"
          "  --disable-audio\n"
          "  --auto-connect             Connect immediately using host/port from player.conf\n"
          "  --verbose\n"
          "  --debug                    Verbose plus per-opcode trace (DC_LOG_DEBUG)\n"
          "  --log-file [path]          Write timestamped diagnostics to a file (useful for a GUI "
          "client with no console)\n"
          "  --version\n"
          "\n"
          "Testing options:\n"
          "  --card-preview             Open a window showing A and 10 of each suit; x/ESC to "
          "exit\n",
          stderr);
      exit(EXIT_FAILURE);
    }
  }
  return cli_args;
}

CliArgs_t parse_server_args(int argc, char *argv[]) {
  enum {
    OPT_LOG_GAME_RESULTS = 1,
    OPT_LOG_HANDS,
    OPT_CONF,
    OPT_BIND,
    OPT_PORT,
    OPT_NAME,
    OPT_VERSION,
    OPT_VERBOSE,
    OPT_DEBUG,
    OPT_DISABLE_TIMEOUT,
    OPT_AUTODEAL,
    OPT_DISABLE_PUBLISH,
    OPT_LOG_FILE,
  };

  static const glopt_option_t options[] = {
      {"log-game-results", GLOPT_REQUIRED_ARG, OPT_LOG_GAME_RESULTS, 0},
      {"log-hands", GLOPT_REQUIRED_ARG, OPT_LOG_HANDS, 0},
      {"conf", GLOPT_REQUIRED_ARG, OPT_CONF, 0},
      {"bind-address", GLOPT_REQUIRED_ARG, OPT_BIND, 0},
      {"port", GLOPT_REQUIRED_ARG, OPT_PORT, 0},
      {"name", GLOPT_REQUIRED_ARG, OPT_NAME, 0},
      {"version", GLOPT_NO_ARG, OPT_VERSION, 0},
      {"verbose", GLOPT_NO_ARG, OPT_VERBOSE, 0},
      {"debug", GLOPT_NO_ARG, OPT_DEBUG, 0},
      {"disable-timeout", GLOPT_NO_ARG, OPT_DISABLE_TIMEOUT, 0},
      {"autodeal", GLOPT_NO_ARG, OPT_AUTODEAL, 0},
      {"disable-publish", GLOPT_NO_ARG, OPT_DISABLE_PUBLISH, 0},
      {"log-file", GLOPT_REQUIRED_ARG, OPT_LOG_FILE, 0},
      {NULL, 0, 0, 0}};

  glopt_parser_t parser;
  glopt_init(&parser, options);
  CliArgs_t cli_args = {0};
  cli_args.run_server_flag = true;

  /* Deterministic test mode (fixed deck, short timeouts) is enabled via the
   * DC_TEST environment variable, not a user-facing flag. */
  const char *dc_test_env = getenv("DC_TEST");
  dc_test_mode = (dc_test_env && strcmp(dc_test_env, "1") == 0);

  int opt;
  while ((opt = glopt_next(&parser, argc, argv)) != -1) {
    switch (opt) {
    case OPT_LOG_GAME_RESULTS:
      cli_args.server_log_game_results_file = parser.optarg;
      break;
    case OPT_LOG_HANDS:
      cli_args.server_log_hands_file = parser.optarg;
      break;
    case OPT_CONF:
      cli_args.server_conf = parser.optarg;
      break;
    case OPT_BIND:
      cli_args.bind_address = parser.optarg;
      break;
    case OPT_PORT: {
      unsigned long port_val;
      parse_unsigned(parser.optarg, UINT16_MAX, &port_val);
      cli_args.port = (uint16_t)port_val;
      break;
    }
    case OPT_NAME:
      cli_args.server_name = parser.optarg;
      break;
    case OPT_VERSION:
      print_version();
      exit(EXIT_SUCCESS);
      break;
    case OPT_VERBOSE:
      verbose = true;
      break;
    case OPT_DEBUG:
      verbose = true;
      dc_debug = true;
      break;
    case OPT_DISABLE_TIMEOUT:
      cli_args.disable_timeout = true;
      break;
    case OPT_AUTODEAL:
      cli_args.autodeal = true;
      cli_args.disable_timeout = true;
      break;
    case OPT_DISABLE_PUBLISH:
      cli_args.disable_publish = true;
      break;
    case OPT_LOG_FILE:
      dc_log_set_file(parser.optarg);
      break;
    case '?':
    default:
      print_version();
      fputs("Usage: " DEALERSCHOICE_NAME "-server [options]\n"
            "  --bind-address [IP]        Address to bind to (default: all interfaces)\n"
            "  --port [port]\n"
            "  --name [name]              Server name shown to clients (overrides conf)\n"
            "  --conf [path]              Alternate server config file\n"
            "  --log-game-results [path]\n"
            "  --verbose\n"
            "  --debug                    Verbose plus per-opcode trace (DC_LOG_DEBUG)\n"
            "  --log-file [path]          Write timestamped diagnostics to a file\n"
            "  --version\n"
            "\n"
            "Testing options:\n"
            "  --log-hands [path]\n"
            "  --disable-timeout          Do not disconnect players who exceed the action "
            "timeout\n"
            "  --autodeal                 Auto-deal hands (implies --disable-timeout)\n",
            stderr);
      exit(EXIT_FAILURE);
    }
  }
  return cli_args;
}
