/*
 server_main.c
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

/* Entry point for the headless dealers-choice-server binary.  It links only
 * libdc_core (no SDL/audio) and is the same server engine the GUI binary runs
 * via --server.  (run_server() handles tcpme_init internally.) */

#include <stdio.h>
#include <stdlib.h>

#ifdef ENABLE_NLS
#include <libintl.h>
#include <locale.h>
#endif

#include <sodium.h>

#include "cli.h"
#include "config.h"
#include "game.h"
#include "server.h"
#include "util.h"

int main(int argc, char *argv[]) {
#ifdef ENABLE_NLS
  char *locale_dir = getenv("DEALERSCHOICE_LOCALEDIR");
  if (!locale_dir)
    locale_dir = DEALERSCHOICE_LOCALEDIR;

  setlocale(LC_ALL, "");
  bindtextdomain(DEALERSCHOICE_NAME, locale_dir);
  textdomain(DEALERSCHOICE_NAME);
#endif

  Path_t path = {0};
  get_data_dir(&path);

  const CliArgs_t cli_args = parse_server_args(argc, argv);

  if (sodium_init() < 0) {
    dc_log(DC_LOG_ERROR, "libsodium init failed");
    return 1;
  }
  pcg_srand_auto();

  return run_server(&cli_args, &path);
}
