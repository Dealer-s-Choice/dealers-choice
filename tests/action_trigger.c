/* Pins the action-button trigger rule (src/ui/game_logic.c).
 *
 * The dispatch this replaced was `PointInRect(mouse_pos, rect) || keysym ==
 * hotkey`, evaluated for mouse AND key events alike. mouse_pos is the live
 * cursor position, unrelated to the event, so ANY keypress fired whichever
 * action the pointer was resting over: pressing a bet-amount digit with the
 * cursor over Raise raised. These cases fail against that version. */

#include "00_test.h"

#include "client_internal.h"

#define HOTKEY SDLK_r
#define OTHER_KEY SDLK_3

static ButtonWidget_t make_button(void) {
  ButtonWidget_t bw = {0};
  bw.base.rect = (SDL_Rect){100, 100, 80, 30};
  bw.hotkey = HOTKEY;
  return bw;
}

static const SDL_Point inside = {120, 110};
static const SDL_Point outside = {10, 10};

static void test_keypress_ignores_cursor_position(void) {
  ButtonWidget_t bw = make_button();
  SDL_Event e = {0};
  e.type = SDL_KEYDOWN;

  /* THE BUG: a key that is not the hotkey, pressed while the pointer happens to
     rest on the button. Must not trigger. */
  e.key.keysym.sym = OTHER_KEY;
  assert(!action_triggered(&e, inside, &bw));
  assert(!action_triggered(&e, outside, &bw));

  /* The hotkey works wherever the pointer is -- the cursor is irrelevant to a
     key event in both directions. */
  e.key.keysym.sym = HOTKEY;
  assert(action_triggered(&e, outside, &bw));
  assert(action_triggered(&e, inside, &bw));
}

static void test_click_requires_left_button_and_the_rect(void) {
  ButtonWidget_t bw = make_button();
  SDL_Event e = {0};
  e.type = SDL_MOUSEBUTTONDOWN;

  e.button.button = SDL_BUTTON_LEFT;
  assert(action_triggered(&e, inside, &bw));
  assert(!action_triggered(&e, outside, &bw));

  /* Right and middle click must not act: too easy to hit by accident on Fold. */
  e.button.button = SDL_BUTTON_RIGHT;
  assert(!action_triggered(&e, inside, &bw));
  e.button.button = SDL_BUTTON_MIDDLE;
  assert(!action_triggered(&e, inside, &bw));
}

static void test_mouse_event_is_not_read_as_a_key(void) {
  ButtonWidget_t bw = make_button();
  SDL_Event e = {0};

  /* event.key aliases the SDL_Event union over the mouse event, so keysym.sym
     on a click reads bytes that are not a keycode -- and can equal a hotkey.
     Set that up deliberately: the click is outside the rect and not the left
     button, so the only thing that could trigger it is the aliased key. */
  e.type = SDL_MOUSEBUTTONDOWN;
  e.key.keysym.sym = HOTKEY;
  e.button.button = SDL_BUTTON_RIGHT;
  assert(!action_triggered(&e, outside, &bw));
}

static void test_other_event_types_never_trigger(void) {
  ButtonWidget_t bw = make_button();
  SDL_Event e = {0};

  /* Motion over the button is hover, not a press. */
  e.type = SDL_MOUSEMOTION;
  assert(!action_triggered(&e, inside, &bw));

  e.type = SDL_KEYUP;
  e.key.keysym.sym = HOTKEY;
  assert(!action_triggered(&e, inside, &bw));
}

_MAIN_HEAD_(void) argc;
(void)argv;
test_keypress_ignores_cursor_position();
test_click_requires_left_button_and_the_rect();
test_mouse_event_is_not_read_as_a_key();
test_other_event_types_never_trigger();
fprintf(stderr, "action-trigger tests: OK\n");
_MAIN_TAIL_
