/* Pins down the text-texture cache in src/widgets/card.c.  The cache
 * skips TTF_RenderUTF8_Blended + SDL_CreateTextureFromSurface when a
 * card's text and color haven't changed since the last render; without
 * it those two calls happen every frame for every card and account for
 * the ~32k allocations/sec the heaptrack profile caught during
 * gameplay.
 *
 * No SDL renderer is initialised: the test only exercises
 * card_widget_text_cache_valid() (pure function) and reads the
 * card_widget_text_cache_misses counter, both exposed in card.h.
 */

#include "00_test.h"

#include "widgets/card.h"

static void test_cache_invalid_when_never_rendered(void) {
  CardWidget_t cw = {0};
  /* No cached texture yet — must be reported invalid so the render
   * path knows to do the (expensive) initial generation. */
  snprintf(cw.text, sizeof(cw.text), "A♠");
  cw.textColor = (SDL_Color){0, 0, 0, 255};
  assert(!card_widget_text_cache_valid(&cw));
}

static void test_cache_valid_when_text_and_color_unchanged(void) {
  CardWidget_t cw = {0};
  snprintf(cw.text, sizeof(cw.text), "A♠");
  cw.textColor = (SDL_Color){0, 0, 0, 255};
  /* Pretend the render path has populated the cache.  Use a non-NULL
   * sentinel pointer for the texture — the validity check only looks
   * at NULL vs not-NULL and never dereferences. */
  cw.cached_text_texture = (SDL_Texture *)(uintptr_t)0xdeadbeef;
  snprintf(cw.cached_text, sizeof(cw.cached_text), "A♠");
  cw.cached_text_color = cw.textColor;
  assert(card_widget_text_cache_valid(&cw));
}

static void test_cache_invalid_when_text_changes(void) {
  CardWidget_t cw = {0};
  snprintf(cw.text, sizeof(cw.text), "K♥");
  cw.textColor = (SDL_Color){255, 0, 0, 255};
  cw.cached_text_texture = (SDL_Texture *)(uintptr_t)0xdeadbeef;
  snprintf(cw.cached_text, sizeof(cw.cached_text), "A♠");
  cw.cached_text_color = cw.textColor;
  /* Same widget instance now displays a different card — the cached
   * texture is stale. */
  assert(!card_widget_text_cache_valid(&cw));
}

static void test_cache_invalid_when_color_changes(void) {
  CardWidget_t cw = {0};
  snprintf(cw.text, sizeof(cw.text), "A♠");
  cw.textColor = (SDL_Color){255, 0, 0, 255}; /* now red */
  cw.cached_text_texture = (SDL_Texture *)(uintptr_t)0xdeadbeef;
  snprintf(cw.cached_text, sizeof(cw.cached_text), "A♠");
  cw.cached_text_color = (SDL_Color){0, 0, 0, 255}; /* was black */
  /* Text unchanged but color did — e.g. wild-card highlight tint flipped.
   * Must invalidate. */
  assert(!card_widget_text_cache_valid(&cw));
}

static void test_alpha_change_invalidates(void) {
  CardWidget_t cw = {0};
  snprintf(cw.text, sizeof(cw.text), "A♠");
  cw.textColor = (SDL_Color){0, 0, 0, 200};
  cw.cached_text_texture = (SDL_Texture *)(uintptr_t)0xdeadbeef;
  snprintf(cw.cached_text, sizeof(cw.cached_text), "A♠");
  cw.cached_text_color = (SDL_Color){0, 0, 0, 255};
  assert(!card_widget_text_cache_valid(&cw));
}

static void test_miss_counter_starts_at_zero(void) {
  /* card_widget_text_cache_misses is process-global.  Other tests in
   * this file don't call card_widget_render (no renderer initialised),
   * so it should stay at zero across this test process. */
  assert(card_widget_text_cache_misses == 0);
}

_MAIN_HEAD_
(void)argc;
(void)argv;
test_cache_invalid_when_never_rendered();
test_cache_valid_when_text_and_color_unchanged();
test_cache_invalid_when_text_changes();
test_cache_invalid_when_color_changes();
test_alpha_change_invalidates();
test_miss_counter_starts_at_zero();
fprintf(stderr, "card-texture-cache: 6 cases OK; miss counter=%lu\n",
        card_widget_text_cache_misses);
_MAIN_TAIL_
