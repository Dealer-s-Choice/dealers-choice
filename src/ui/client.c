/*
 client.c
 https://github.com/Dealer-s-Choice/dealers_choice

 MIT License

 Copyright (c) 2025,2026 Andy Alt

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
#ifdef _WIN32
#include "dc_windows.h"
#else
#include <dirent.h>
#endif
#include <math.h>
#include <stdatomic.h>

#include <deckhandler.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "client.h"
#include "client_internal.h"
#include "game.h"
#include "globals_gui.h"
#include "graphics.h"
#include "hotkeys.h"
#include "widgets/button.h"
#include "widgets/card_text_atlas.h"
#include "widgets/dealer.h"
#include "widgets/image.h"
#include "widgets/indicator.h"
#include "widgets/nick.h"
#include "widgets/ping.h"
#include "widgets/step_scale.h"
#include "widgets/text.h"

#include "util.h"

#include <sodium.h>

// What's the max this needs to be to support the unicode suit symbol?
// SIZEOF_CARD_TEXT is defined in client.h

#define AUDIO_SLOW_WARN_MS 500 /* audio engine init/uninit blocking >= this (#307) */

#define MAX_COIN_IMAGES 16

void ma_sound_start_wrap(ma_sound *pSound, const char *file, const int line) {
  ma_result result = ma_sound_start(pSound);
  if (result != MA_SUCCESS) {
    fprintf(stderr, "[ma_sound_start] Failed (%s:%d) -> result = %d\n", file, line, result);
  }
}

static SoundContext_t *g_sound_context = NULL;
static atomic_bool g_audio_needs_restart;
static atomic_bool g_audio_shutting_down;

static void on_audio_device_notification(const ma_device_notification *pNotification) {
  if (!g_sound_context || atomic_load(&g_audio_shutting_down))
    return;
  if (pNotification->type == ma_device_notification_type_stopped) {
    verbose_puts("Audio device stopped; will attempt restart");
    atomic_store(&g_audio_needs_restart, true);
  }
}

void detect_player_changes(const GameState_t *gs, bool was_connected[MAX_PLAYERS],
                           bool joined[MAX_PLAYERS], bool left[MAX_PLAYERS]) {
  for (int i = 0; i < MAX_PLAYERS; i++) {
    bool now = gs->player[i].is_connected;
    joined[i] = !was_connected[i] && now;
    left[i] = was_connected[i] && !now;
    was_connected[i] = now;
  }
}

SDL_Texture *load_coin_texture(SDL_Renderer *renderer, const char *base_path, const char *coin) {
  const char *subdir = "/images/";
  size_t len = strlen(base_path) + strlen(subdir) + strlen(coin) + 1;

  char *full_path = calloc_wrap(len, 1);
  snprintf(full_path, len, "%s%s%s", base_path, subdir, coin);

  SDL_Texture *tex = load_texture(renderer, full_path);
  free(full_path);
  return tex;
}

/* Reads {base_path}/images/coins/, loads up to max_count .png files as
 * textures into out[]. Returns the number loaded. Warns if capped. */
static size_t load_coin_textures(SDL_Renderer *renderer, const char *base_path, SDL_Texture **out,
                                 size_t max_count) {
  const char *suffix = "/images/coins";
  char *dirpath = calloc_wrap(strlen(base_path) + strlen(suffix) + 1, 1);
  snprintf(dirpath, strlen(base_path) + strlen(suffix) + 1, "%s%s", base_path, suffix);

  size_t i = 0;

#ifdef _WIN32
  char pattern[512];
  snprintf(pattern, sizeof pattern, "%s\\*.png", dirpath);
  free(dirpath);
  WIN32_FIND_DATAA fd;
  HANDLE h = FindFirstFileA(pattern, &fd);
  if (h == INVALID_HANDLE_VALUE)
    return 0;
  do {
    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
      continue;
    if (i >= max_count) {
      fprintf(stderr, "Warning: more than %zu coin images found; increase MAX_COIN_IMAGES\n",
              max_count);
      break;
    }
    char rel[512];
    snprintf(rel, sizeof rel, "coins/%s", fd.cFileName);
    out[i++] = load_coin_texture(renderer, base_path, rel);
  } while (FindNextFileA(h, &fd));
  FindClose(h);
#else
  DIR *d = opendir(dirpath);
  free(dirpath);
  if (!d)
    return 0;
  struct dirent *ent;
  while ((ent = readdir(d)) != NULL) {
    size_t nlen = strlen(ent->d_name);
    if (nlen <= 4 || strcmp(ent->d_name + nlen - 4, ".png") != 0)
      continue;
    if (i >= max_count) {
      fprintf(stderr, "Warning: more than %zu coin images found; increase MAX_COIN_IMAGES\n",
              max_count);
      break;
    }
    char rel[512];
    snprintf(rel, sizeof rel, "coins/%s", ent->d_name);
    out[i++] = load_coin_texture(renderer, base_path, rel);
  }
  closedir(d);
#endif

  return i;
}

typedef struct {
  const char *host_str;
  uint16_t port;
  tcpme_socket_t sock;
  SDL_atomic_t done;
  SDL_atomic_t orphaned;
} ConnectAttempt_t;

static int connect_thread_fn(void *data) {
  ConnectAttempt_t *ca = data;
  ca->sock = tcpme_connect(ca->host_str, ca->port);
  if (tcpme_socket_valid(ca->sock))
    tcpme_set_timeout(ca->sock, SOCKET_IO_TIMEOUT_MS);
  SDL_AtomicSet(&ca->done, 1);
  if (SDL_AtomicGet(&ca->orphaned)) {
    if (tcpme_socket_valid(ca->sock))
      tcpme_close(ca->sock);
    SDL_free(ca);
  }
  return 0;
}

typedef struct {
  ma_engine_config engineConfig;
  ma_engine *engine;
  ma_result result;
  SDL_atomic_t done;
} AudioInitAttempt_t;

static int audio_init_thread_fn(void *data) {
  AudioInitAttempt_t *aa = data;
  uint32_t t0 = SDL_GetTicks();
  aa->result = ma_engine_init(&aa->engineConfig, aa->engine);
  uint32_t took = SDL_GetTicks() - t0;
  if (took >= AUDIO_SLOW_WARN_MS)
    dc_log(DC_LOG_WARN, "audio engine init took %ums (slow audio backend)", took);
  SDL_AtomicSet(&aa->done, 1);
  return 0;
}

typedef struct {
  ma_engine *engine;
  SDL_atomic_t done;
} AudioUninitAttempt_t;

static int audio_uninit_thread_fn(void *data) {
  AudioUninitAttempt_t *au = data;
  uint32_t t0 = SDL_GetTicks();
  ma_engine_uninit(au->engine);
  uint32_t took = SDL_GetTicks() - t0;
  if (took >= AUDIO_SLOW_WARN_MS)
    dc_log(DC_LOG_WARN, "audio engine uninit took %ums (slow audio backend)", took);
  SDL_AtomicSet(&au->done, 1);
  return 0;
}

bool get_socket_context_and_run_client(PlayerConfig_t *player_config, const CliArgs_t *cli_args,
                                       const char *host_str, const uint16_t port,
                                       SdlContext_t *sdl_context, Font_t *font, Path_t *path,
                                       LinkWidget_t **links, SocketContext_t *out_socket_context) {
  SocketContext_t socket_context = {0};

  // tcpme_connect blocks for the OS TCP timeout on unreachable hosts.
  // Run each attempt on a background thread; heap-allocate the state so we
  // can safely SDL_DetachThread (rather than WaitThread) on cancel/timeout.
  // Per-attempt timeout keeps the counter advancing on slow/unreachable hosts.
  static const Uint32 ATTEMPT_TIMEOUT_MS = 5000;

  ButtonWidget_t *btn_cancel = NULL;
  TextWidget_t *status_tw = NULL;
  if (sdl_context && font) {
    btn_cancel = button_widget_create_styled(_("Cancel"), &ROLE_PRIMARY, font->fonts, SDLK_ESCAPE);
    if (btn_cancel) {
      btn_cancel->base.rect.x = g_center.x - btn_cancel->base.rect.w / 2;
      btn_cancel->base.rect.y = g_center.y + 60;
    }
    char initial_status[256] = {0};
    snprintf(initial_status, sizeof(initial_status), _("Attempting connection to: %s... (%d/%d)"),
             host_str, 1, player_config->connect_attempts);
    status_tw = text_widget_create(initial_status, font->fonts[FONT_DEFAULT], DC_TEXT_ON_DARK);
    if (status_tw) {
      status_tw->base.rect.x = 10;
      status_tw->base.rect.y = g_center.y;
    }
  }

  bool cancelled = false;
  bool sdl_quit = false;
  uint8_t attempts;
  for (attempts = 0; attempts < player_config->connect_attempts; ++attempts) {
    if (status_tw && attempts > 0) {
      char tmp[256] = {0};
      snprintf(tmp, sizeof(tmp), _("Attempting connection to: %s... (%d/%d)"), host_str,
               attempts + 1, player_config->connect_attempts);
      text_widget_set_text(status_tw, tmp);
      status_tw->base.rect.x = 10;
      status_tw->base.rect.y = g_center.y;
    }

    ConnectAttempt_t *ca = SDL_malloc(sizeof(ConnectAttempt_t));
    if (!ca)
      break;
    ca->host_str = host_str;
    ca->port = port;
    ca->sock = TCPME_INVALID_SOCKET;
    SDL_AtomicSet(&ca->done, 0);
    SDL_AtomicSet(&ca->orphaned, 0);

    SDL_Thread *thread = SDL_CreateThread(connect_thread_fn, "tcp_connect", ca);
    if (!thread) {
      // Fallback: blocking connect with no event handling this attempt
      ca->sock = tcpme_connect(host_str, port);
      if (tcpme_socket_valid(ca->sock))
        tcpme_set_timeout(ca->sock, SOCKET_IO_TIMEOUT_MS);
      SDL_AtomicSet(&ca->done, 1);
    }

    Uint32 attempt_start = SDL_GetTicks();
    bool timed_out = false;

    while (!SDL_AtomicGet(&ca->done) && !cancelled && !sdl_quit) {
      if (SDL_GetTicks() - attempt_start >= ATTEMPT_TIMEOUT_MS) {
        timed_out = true;
        break;
      }
      if (sdl_context) {
        clear_screen(sdl_context->renderer);
        if (status_tw)
          status_tw->base.render(&status_tw->base);
        if (btn_cancel)
          ui_widget_render(&btn_cancel->base);
        SDL_RenderPresent(sdl_context->renderer);
      }
      SDL_Event e;
      while (SDL_PollEvent(&e)) {
        SDL_Point mp = {e.button.x, e.button.y};
        if (e.type == SDL_QUIT) {
          sdl_quit = true;
        } else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
          cancelled = true;
        } else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
          if (btn_cancel && SDL_PointInRect(&mp, &btn_cancel->base.rect))
            cancelled = true;
        } else if (e.type == SDL_MOUSEMOTION) {
          SDL_Point mmp = {e.motion.x, e.motion.y};
          if (btn_cancel)
            btn_cancel->base.hovered = SDL_PointInRect(&mmp, &btn_cancel->base.rect);
        }
      }
      SDL_Delay(16);
    }

    if (thread && !SDL_AtomicGet(&ca->done)) {
      // Thread is still running (cancelled or timed out). Mark it orphaned so
      // the thread closes any socket it opens and frees ca itself.
      SDL_AtomicSet(&ca->orphaned, 1);
      SDL_DetachThread(thread);
      thread = NULL;
    } else {
      // Thread finished normally; safe to wait and free.
      if (thread)
        SDL_WaitThread(thread, NULL);
      tcpme_socket_t s = ca->sock;
      SDL_free(ca);
      ca = NULL;
      if (tcpme_socket_valid(s)) {
        socket_context.sock = s;
        break;
      }
    }

    if (cancelled || sdl_quit)
      break;

    if (!timed_out)
      fprintf(stderr, "Attempt %d: Failed to connect to server: %s\n", attempts + 1,
              tcpme_get_error());

    // Wait out the remainder of ATTEMPT_TIMEOUT_MS so each attempt cycle
    // takes the full 5 seconds even when the connect fails immediately
    // (e.g. ECONNREFUSED). Skip on last attempt.
    if (attempts < (uint8_t)(player_config->connect_attempts - 1)) {
      Uint32 elapsed = SDL_GetTicks() - attempt_start;
      Uint32 wait_ms = elapsed < ATTEMPT_TIMEOUT_MS ? ATTEMPT_TIMEOUT_MS - elapsed : 0;
      Uint32 start = SDL_GetTicks();
      while (SDL_GetTicks() - start < wait_ms && !cancelled && !sdl_quit) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
          SDL_Point mp = {e.button.x, e.button.y};
          if (e.type == SDL_QUIT) {
            sdl_quit = true;
          } else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
            cancelled = true;
          } else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
            if (btn_cancel && SDL_PointInRect(&mp, &btn_cancel->base.rect))
              cancelled = true;
          } else if (e.type == SDL_MOUSEMOTION) {
            SDL_Point mmp = {e.motion.x, e.motion.y};
            if (btn_cancel)
              btn_cancel->base.hovered = SDL_PointInRect(&mmp, &btn_cancel->base.rect);
          }
        }
        SDL_Delay(16);
      }
    }
  }

  if (btn_cancel)
    ui_widget_destroy(&btn_cancel->base);
  if (status_tw)
    ui_widget_destroy(&status_tw->base);

  if (!tcpme_socket_valid(socket_context.sock)) {
    if (!cancelled && !sdl_quit)
      printf("All %d attempts failed.\n", attempts);
    return !sdl_quit;
  }

  tcpme_socket_t sock = socket_context.sock;
  socket_context.set = tcpme_alloc_set(1);
  if (!socket_context.set) {
    fprintf(stderr, "Failed to allocate socket set: %s\n", tcpme_get_error());
    tcpme_close(sock);
    return false;
  }

  if (tcpme_add_socket(socket_context.set, sock) != 0)
    fputs("Socket set full\n", stderr);

  if (send_protocol_header(sock, 0) != 0)
    goto cleanup;

  if (!dc_test_mode) {
    const char *env_pw = getenv("DC_PASSWORD");
    const char *password = env_pw ? env_pw : player_config->password;
    if (authenticate_with_server(sock, password) < 0)
      fprintf(stderr, "Authentication attempt failed\n");

    GameState_t game_state = {0};
    GameSettings_t game_settings = {0};
    ClientState_t client_state = {0};
    char *nick = player_config->nick;
    uint16_t len = (uint16_t)strlen(nick);
    uint16_t net_len = SDL_SwapBE16(len);
    send_all_tcp(sock, &net_len, sizeof(net_len));
    if (send_all_tcp(sock, player_config->nick, len) != 0)
      fprintf(stderr, "Failed to send player nick to server\n");

    const Uint32 timeout = 2000;    // 2 seconds
    const Uint32 retry_delay = 100; // milliseconds per retry

    Uint32 start_time = SDL_GetTicks(); // milliseconds
    ERecvStatus_t recv_status;
    do {
      recv_status = recv_game_settings(sock, socket_context.set, &game_settings);
      if (recv_status == RECV_SUCCESS) {
        break;
      } else if (recv_status == RECV_ERROR) {
        fprintf(stderr, "Failed to receive game settings\n");
        exit(EXIT_FAILURE);
      }

      SDL_Delay(retry_delay);
    } while (SDL_GetTicks() - start_time < timeout);

    start_time = SDL_GetTicks(); // milliseconds
    do {
      recv_status =
          recv_game_state(&socket_context, &game_state, &client_state, game_settings.client_id);

      if (recv_status == RECV_SUCCESS) {
        break;
      } else if (recv_status == RECV_ERROR) {
        fprintf(stderr, "Failed to receive initial game state\n");
        exit(EXIT_FAILURE);
      }

      SDL_Delay(retry_delay);
    } while (SDL_GetTicks() - start_time < timeout);
    if (recv_status != RECV_SUCCESS)
      exit(EXIT_FAILURE);

    bool went_back_result = false;
    bool audio_sdl_quit = false;
    size_t i;
    size_t n_sounds_init = 0;
    size_t n_coin_sounds_init = 0;
    size_t n_coin_images = 0;
    SDL_Texture *coin_textures[MAX_COIN_IMAGES] = {NULL};
    atomic_store(&g_audio_needs_restart, false);
    atomic_store(&g_audio_shutting_down, false);
    SoundContext_t sound_context = {0};
    sound_context.engineConfig = ma_engine_config_init();
    if (player_config->volume == 0 || cli_args->disable_audio) {
      sound_context.engineConfig.noDevice = MA_TRUE;
      sound_context.engineConfig.channels = 2;
      sound_context.engineConfig.sampleRate = 48000;
      sound_context.result = ma_engine_init(&sound_context.engineConfig, &sound_context.engine);
    } else {
      // ma_engine_init can block for seconds when a sound server (e.g. PulseAudio)
      // is unreachable. Run it on a background thread so the window stays responsive.
      verbose_puts("Initializing audio engine (powered by miniaudio: https://miniaud.io/)");
      sound_context.engineConfig.notificationCallback = on_audio_device_notification;
      AudioInitAttempt_t aa = {.engineConfig = sound_context.engineConfig,
                               .engine = &sound_context.engine};
      SDL_AtomicSet(&aa.done, 0);
      SDL_Thread *audio_thread = SDL_CreateThread(audio_init_thread_fn, "audio_init", &aa);
      if (!audio_thread) {
        aa.result = ma_engine_init(&aa.engineConfig, aa.engine);
        SDL_AtomicSet(&aa.done, 1);
      }
      while (!SDL_AtomicGet(&aa.done)) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
          if (e.type == SDL_QUIT)
            audio_sdl_quit = true;
        }
        SDL_Delay(16);
      }
      if (audio_thread)
        SDL_WaitThread(audio_thread, NULL);
      sound_context.result = aa.result;
      if (sound_context.result != MA_SUCCESS) {
        fprintf(
            stderr,
            "Warning: Failed to initialize audio engine (code: %d), continuing without audio.\n",
            sound_context.result);
        sound_context.engineConfig.noDevice = MA_TRUE;
        sound_context.engineConfig.channels = 2;
        sound_context.engineConfig.sampleRate = 48000;
        sound_context.engineConfig.notificationCallback = NULL;
        sound_context.result = ma_engine_init(&sound_context.engineConfig, &sound_context.engine);
      }
    }
    if (sound_context.result != MA_SUCCESS) {
      fprintf(stderr, "Error: Failed to initialize audio engine (code: %d).\n",
              sound_context.result);
      exit(EXIT_FAILURE);
    }
    if (audio_sdl_quit)
      goto cleanup_audio;
    if (!sound_context.engineConfig.noDevice)
      g_sound_context = &sound_context;
    ma_engine_set_volume(&sound_context.engine, player_config->volume * .1f);

    // Using {0} or {{0}} for the The ma_sound field initializer doesn't work so
    // using 'ma_tmp' instead
    ma_sound ma_tmp = {0};
    Sound_t sounds[] = {[SND_SERVER_JOIN] = {"server_join.wav", ma_tmp},
                        [SND_MY_TURN] = {"my_turn.wav", ma_tmp},
                        [SND_MY_TURN_LAST_CHANCE] = {"my_turn_last_chance.wav", ma_tmp},
                        [SND_GAME_OVER] = {"game_over.wav", ma_tmp}};

    Sound_t coin_hit_sounds[] = {
        {"coin_hit_001.wav", ma_tmp}, {"coin_hit_002.wav", ma_tmp}, {"coin_hit_003.wav", ma_tmp},
        {"coin_hit_004.wav", ma_tmp}, {"coin_hit_005.wav", ma_tmp}, {"coin_hit_006.wav", ma_tmp},
        {"coin_hit_007.wav", ma_tmp},
    };

    sound_context.coin_array_size = ARRAY_SIZE(coin_hit_sounds);

    sound_context.sounds = sounds;
    sound_context.coin_hit_sounds = coin_hit_sounds;
    for (i = 0; i < SND_NUM_SOUNDS; i++) {
      char *sub = canfigger_path_join("sounds", sounds[i].filename);
      char *snd_path = canfigger_path_join(path->data, sub);
      free(sub);
      if (!snd_path) {
        fprintf(stderr, "Failed to build sound path %zd\n", i);
        goto cleanup_audio;
      }
      bool ok = ma_sound_init_from_file(&sound_context.engine, snd_path, 0, NULL, NULL,
                                        &sounds[i].sound) == MA_SUCCESS;
      free(snd_path);
      if (!ok) {
        fprintf(stderr, "Failed to init sound %zd\n", i);
        goto cleanup_audio;
      }
      n_sounds_init++;
    }

    for (i = 0; i < ARRAY_SIZE(coin_hit_sounds); i++) {
      char *sub = canfigger_path_join("sounds/coin", coin_hit_sounds[i].filename);
      char *snd_path = canfigger_path_join(path->data, sub);
      free(sub);
      if (!snd_path) {
        fprintf(stderr, "Failed to build sound path %zd\n", i);
        goto cleanup_audio;
      }
      bool ok = ma_sound_init_from_file(&sound_context.engine, snd_path, 0, NULL, NULL,
                                        &coin_hit_sounds[i].sound) == MA_SUCCESS;
      free(snd_path);
      if (!ok) {
        fprintf(stderr, "Failed to init sound %zd\n", i);
        goto cleanup_audio;
      }
      n_coin_sounds_init++;
    }

    n_coin_images =
        load_coin_textures(sdl_context->renderer, path->data, coin_textures, MAX_COIN_IMAGES);

    {
      bool running = true;
      bool went_back = false;
      do {
        if (atomic_exchange(&g_audio_needs_restart, false)) {
          if (ma_engine_start(&sound_context.engine) != MA_SUCCESS)
            fputs("Warning: failed to restart audio engine after device change\n", stderr);
          else
            verbose_puts("Audio engine restarted after device change");
        }
        EGameSelResult_t sel = handle_game_selection(
            player_config, &socket_context, game_settings.client_id, &game_state, &client_state,
            sdl_context, font, &sound_context, links, path);
        if (sel == GAME_SEL_BACK || sel == GAME_SEL_ERROR || sel == GAME_SEL_QUIT) {
          went_back = true;
          break;
        }

        EGameLogicResult_t result = handle_game_logic(
            player_config, &socket_context, &game_settings, &game_state, sdl_context, font, path,
            &sound_context, coin_textures, n_coin_images);
        if (result == GAME_LOGIC_DISCONNECTED)
          went_back = true;
        running = (result == GAME_LOGIC_AT_MENU);
      } while (running);
      went_back_result = went_back;
    }
  cleanup_audio:
    for (i = 0; i < n_sounds_init; i++)
      ma_sound_uninit(&sounds[i].sound);
    for (i = 0; i < n_coin_sounds_init; i++)
      ma_sound_uninit(&coin_hit_sounds[i].sound);
    for (i = 0; i < n_coin_images; i++)
      SDL_DestroyTexture(coin_textures[i]);
    atomic_store(&g_audio_shutting_down, true);
    g_sound_context = NULL;
    {
      AudioUninitAttempt_t au;
      au.engine = &sound_context.engine;
      SDL_AtomicSet(&au.done, 0);
      SDL_Thread *uninit_thread = SDL_CreateThread(audio_uninit_thread_fn, "audio_uninit", &au);
      if (!uninit_thread) {
        ma_engine_uninit(&sound_context.engine);
      } else {
        while (!SDL_AtomicGet(&au.done)) {
          SDL_PumpEvents();
          SDL_Delay(16);
        }
        SDL_WaitThread(uninit_thread, NULL);
      }
    }
    socket_cleanup(&socket_context);

    return went_back_result;
  } else {
    if (out_socket_context)
      *out_socket_context = socket_context;
    return false;
  }

cleanup:
  socket_cleanup(&socket_context);
  return false;
}

void do_sdl_cleanup(SdlContext_t *sdl_context) {
  /* Atlas textures are tied to sdl_context->renderer — must be freed
   * before the renderer they were created with. */
  card_text_atlas_destroy();
  SDL_DestroyRenderer(sdl_context->renderer);
  SDL_DestroyWindow(sdl_context->window);
  SDL_Quit();
}
