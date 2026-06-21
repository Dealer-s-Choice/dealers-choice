/*
 main.h
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

#ifndef _MAIN_H
#define _MAIN_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  const char *host, *server_conf, *bind_address;
  const char *server_name; /* --name: overrides server_name from the conf (#33) */
  uint16_t port;
  uint16_t discovery_port; /* --discovery-port: overrides common.conf lan_discovery_port (#33) */
  const char *server_log_game_results_file;
  const char *server_log_hands_file;
  bool run_server_flag;
  bool disable_audio;
  bool disable_timeout;
  bool autodeal;
  bool auto_connect;
  bool card_preview;
  bool disable_publish; /* server: don't announce to any registry (#33) */
  bool disable_registry_browser; /* client: don't query the registry for the server list (#33) */
} CliArgs_t;

#endif
