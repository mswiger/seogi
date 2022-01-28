#include <argp.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>

#include "dbus.h"
#include "event-loop.h"
#include "seat.h"
#include "seogi.h"
#include "utf8.h"

#include "input-method-unstable-v2-client-protocol.h"
#include "virtual-keyboard-unstable-v1-client-protocol.h"

static void registry_handle_global(
  void *data,
  struct wl_registry *registry,
  uint32_t name,
  const char *interface,
  uint32_t version
) {
  struct seogi_state *state = data;
  
  if (strcmp(interface, wl_seat_interface.name) == 0) {
    struct wl_seat *seat = wl_registry_bind(registry, name, &wl_seat_interface, 7);
    seogi_create_seat(state, seat);
  } else if (strcmp(interface, zwp_input_method_manager_v2_interface.name) == 0) {
    state->input_method_manager = wl_registry_bind(registry, name, &zwp_input_method_manager_v2_interface, 1);
  } else if (strcmp(interface, zwp_virtual_keyboard_manager_v1_interface.name) == 0) {
    state->virtual_keyboard_manager = wl_registry_bind(registry, name, &zwp_virtual_keyboard_manager_v1_interface, 1);
  }
}

static void registry_handle_global_remove(void *data, struct wl_registry *registry, uint32_t name) {
  // TODO
}

static const struct wl_registry_listener registry_listener = {
  .global = registry_handle_global,
  .global_remove = registry_handle_global_remove,
};

static bool handle_key_pressed(struct seogi_seat *seat, xkb_keycode_t xkb_key) {
  bool handled = false;
  xkb_keysym_t sym = xkb_state_key_get_one_sym(seat->xkb_state, xkb_key);

  if (sym == seat->state->toggle_key) {
    seat->enabled = !seat->enabled;
    seogi_emit_seat_status_changed(seat);
    if (!seat->enabled) {
      hangul_ic_reset(seat->input_context);
    }
    handled = true;
  } else {
    switch (sym) {
    case XKB_KEY_space ... XKB_KEY_asciitilde:
    case XKB_KEY_Home ... XKB_KEY_Begin:
      uint32_t ch = xkb_state_key_get_utf32(seat->xkb_state, xkb_key);
      handled = seat->enabled && hangul_ic_process(seat->input_context, ch);
      break;
    case XKB_KEY_BackSpace:
      handled = seat->enabled && hangul_ic_backspace(seat->input_context);
      break;
    default:
      return false;
    }
  }

  const ucschar *commit_ucsstr = hangul_ic_get_commit_string(seat->input_context);
  if (commit_ucsstr[0] != 0) {
    char *commit_str = ucsstr_to_str(commit_ucsstr);
    zwp_input_method_v2_commit_string(seat->input_method, commit_str);
    free(commit_str);
  }

  // Only set the pre-edit string in Hangul mode and when the key has been handled (except the toggle key).
  // Setting the pre-edit string has the side-effect of erasing any selected text. Thus, if the pre-edit string is set
  // unnecessarily, any selected text will be lost.
  if (seat->enabled && handled && sym != seat->state->toggle_key) {
    const ucschar *preedit_ucsstr = hangul_ic_get_preedit_string(seat->input_context);
    char *preedit_str = ucsstr_to_str(preedit_ucsstr);
    zwp_input_method_v2_set_preedit_string(seat->input_method, preedit_str, 0, strlen(preedit_str));
    free(preedit_str);
  }

  zwp_input_method_v2_commit(seat->input_method, seat->serial);

  if (handled) {
    key_buffer_insert(seat->handled_keys, xkb_key);
  }
  key_buffer_insert(seat->pressed_keys, xkb_key);

  return handled;
}

static bool handle_key_released(struct seogi_seat *seat, xkb_keycode_t xkb_key) {
  bool handled = key_buffer_remove(seat->handled_keys, xkb_key);
  key_buffer_remove(seat->pressed_keys, xkb_key);
  return handled;
}

static void handle_key(
  void *data,
  struct zwp_input_method_keyboard_grab_v2 *keyboard_grab,
  uint32_t serial,
  uint32_t time,
  uint32_t key,
  uint32_t state
) {
  struct seogi_seat *seat = data;
  xkb_keycode_t xkb_key = key + 8;

  if (seat->xkb_state == NULL) {
    return;
  }

  bool handled = false;
  switch (state) {
  case WL_KEYBOARD_KEY_STATE_PRESSED:
    handled = handle_key_pressed(seat, xkb_key);
    break;
  case WL_KEYBOARD_KEY_STATE_RELEASED:
    handled = handle_key_released(seat, xkb_key);
    break;
  }

  if (!handled) {
    zwp_virtual_keyboard_v1_key(seat->virtual_keyboard, time, key, state);
  }
}

static void handle_modifiers(
  void *data,
  struct zwp_input_method_keyboard_grab_v2 *keyboard_grab,
  uint32_t serial,
  uint32_t mods_depressed,
  uint32_t mods_latched,
  uint32_t mods_locked,
  uint32_t group
) {
  struct seogi_seat *seat = data;

  if (seat->xkb_state == NULL) {
    return;
  }

  xkb_state_update_mask(seat->xkb_state, mods_depressed, mods_latched, mods_locked, 0, 0, group);
  zwp_virtual_keyboard_v1_modifiers(seat->virtual_keyboard, mods_depressed, mods_latched, mods_locked, group);
}

static void handle_keymap(
  void *data,
  struct zwp_input_method_keyboard_grab_v2 *keyboard_grab,
  uint32_t format,
  int32_t fd,
  uint32_t size
) {
  struct seogi_seat *seat = data;

  char *str = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (str == MAP_FAILED) {
    perror("mmap");
    close(fd);
    return;
  }

  // This is currently needed to avoid the keymap loop bug in Sway
  if (!(seat->xkb_keymap_string == NULL || strcmp(seat->xkb_keymap_string, str) != 0)) {
    munmap(str, size);
    close(fd);
    return;
  }

  zwp_virtual_keyboard_v1_keymap(seat->virtual_keyboard, format, fd, size);

  free(seat->xkb_keymap_string);
  seat->xkb_keymap_string = strdup(str);

  if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
    close(fd);
    return;
  }
  if (seat->xkb_keymap != NULL) {
    xkb_keymap_unref(seat->xkb_keymap);
  }
  
  seat->xkb_keymap = xkb_keymap_new_from_string(
    seat->xkb_context,
    str,
    XKB_KEYMAP_FORMAT_TEXT_V1,
    XKB_KEYMAP_COMPILE_NO_FLAGS
  );
  munmap(str, size);
  close(fd);

  if (seat->xkb_keymap == NULL) {
    fprintf(stderr, "Failed to compile keymap\n");
    return;
  }

  if (seat->xkb_state != NULL) {
    xkb_state_unref(seat->xkb_state);
  }

  seat->xkb_state = xkb_state_new(seat->xkb_keymap);

  if (seat->xkb_state == NULL) {
    fprintf(stderr, "Failed to create XKB state\n");
    return;
  }
}

static void handle_repeat_info(
  void *data,
  struct zwp_input_method_keyboard_grab_v2 *keyboard_grab,
  int32_t rate,
  int32_t delay
) {
  // TODO: implement repeating keypresses (i.e., when key is held down)
}

static const struct zwp_input_method_keyboard_grab_v2_listener keyboard_grab_listener = {
  .key = handle_key,
  .modifiers = handle_modifiers,
  .keymap = handle_keymap,
  .repeat_info = handle_repeat_info,
};

static void handle_activate(void *data, struct zwp_input_method_v2 *input_method) {
  struct seogi_seat *seat = data;
  seat->pending_activate = true;
}

static void handle_deactivate(void *data, struct zwp_input_method_v2 *input_method) {
  struct seogi_seat *seat = data;
  seat->pending_deactivate = true;
}

static void handle_surrounding_text(
  void *data,
  struct zwp_input_method_v2 *input_method,
  const char *text,
  uint32_t cursor,
  uint32_t anchor
) {
  // No-op
}

static void handle_text_change_cause(void *data, struct zwp_input_method_v2 *input_method, uint32_t cause) {
  // No-op
}

static void handle_content_type(
  void *data,
  struct zwp_input_method_v2 *input_method,
  uint32_t hint,
  uint32_t purpose
) {
  // No-op
}

static void handle_done(void *data, struct zwp_input_method_v2 *input_method) {
  struct seogi_seat *seat = data;
  seat->serial++;

  if (seat->pending_activate && !seat->active) {
    seat->keyboard_grab = zwp_input_method_v2_grab_keyboard(input_method);
    zwp_input_method_keyboard_grab_v2_add_listener(seat->keyboard_grab, &keyboard_grab_listener, seat);
    seat->active = true;
  } else if (seat->pending_deactivate && seat->active)  {
    // Make sure to send 'release key' events for keys that are pressed when the "done" event is
    // received. It seems that releasing keys after the 'done' event is received causes the
    // "key released" key events to be swallowed, which makes Wayland clients behave strangely.
    for (size_t i = 0; i < KEY_BUFFER_ITER_LENGTH; i++) {
      if (seat->pressed_keys[i] != 0) {
        uint32_t key = seat->pressed_keys[i] - 8;
        zwp_virtual_keyboard_v1_key(seat->virtual_keyboard, 0, key, WL_KEYBOARD_KEY_STATE_RELEASED);
      }
    }

    zwp_input_method_keyboard_grab_v2_release(seat->keyboard_grab);
    hangul_ic_reset(seat->input_context);
    key_buffer_clear(seat->handled_keys);
    key_buffer_clear(seat->pressed_keys);
    seat->keyboard_grab = NULL;
    seat->active = false;
  }

  seat->pending_activate = false;
  seat->pending_deactivate = false;
}

static void handle_unavailable(void *data, struct zwp_input_method_v2 *input_method) {
  // No-op
}

static const struct zwp_input_method_v2_listener input_method_listener = {
  .activate = handle_activate,
  .deactivate = handle_deactivate,
  .surrounding_text = handle_surrounding_text,
  .text_change_cause = handle_text_change_cause,
  .content_type = handle_content_type,
  .done = handle_done,
  .unavailable = handle_unavailable,
};

static char doc[] =
  "Hangul IME for Wayland\v"
  "See xkbcommon/xkbcommon-keysyms.h for possible toggle key names (remove the XKB_KEY_ prefix).";

static struct argp_option options[] = {
  { "hangul", 'H', 0, 0, "If specified, start in hangul input mode", 0 },
  { "toggle-key", 'k', "KEY", 0, "Key to toggle hangul input (default: Hangul)", 0 },
  { 0 },
};

static error_t parse_opt(int key, char *arg, struct argp_state *state) {
  struct seogi_state *seogi_state = state->input;
  switch (key) {
  case 'H':
    seogi_state->enabled_by_default = true;
    break;
  case 'k':
    seogi_state->toggle_key = xkb_keysym_from_name(arg, XKB_KEYSYM_NO_FLAGS);
    if (seogi_state->toggle_key == XKB_KEY_NoSymbol) {
      argp_failure(state, 1, 0, "`%s' is not a valid key name", arg);
    }
    break;
  default:
    return ARGP_ERR_UNKNOWN;
  }
  return 0;
}

static struct argp argp = { options, parse_opt, 0, doc, 0, 0, 0 };

int main(int argc, char *argv[]) {
  struct seogi_state state = { 0 };
  state.toggle_key = XKB_KEY_Hangul;
  wl_list_init(&state.seats);

  argp_parse(&argp, argc, argv, 0, 0, &state);

  seogi_init_dbus(&state);

  state.display = wl_display_connect(NULL);
  if (state.display == NULL) {
    fprintf(stderr, "failed to connect to Wayland display\n");
    return 1;
  }

  struct wl_registry *registry = wl_display_get_registry(state.display);
  wl_registry_add_listener(registry, &registry_listener, &state);
  wl_display_roundtrip(state.display);

  if (state.input_method_manager == NULL) {
    fprintf(stderr, "unable to initialize zwp_input_method_manager_v2\n");
    return 1;
  }

  struct seogi_seat *seat;
  wl_list_for_each(seat, &state.seats, link) {
    seat->input_context = hangul_ic_new("2"); // TODO: Allow for other keyboard types
    seat->input_method = zwp_input_method_manager_v2_get_input_method(state.input_method_manager, seat->wl_seat);
    zwp_input_method_v2_add_listener(seat->input_method, &input_method_listener, seat);
    seat->virtual_keyboard = zwp_virtual_keyboard_manager_v1_create_virtual_keyboard(
      state.virtual_keyboard_manager,
      seat->wl_seat
    );
    seat->xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    seat->enabled = state.enabled_by_default;
  }

  struct seogi_event_loop loop = { 0 };
  seogi_init_event_loop(&loop, state.bus, state.display);
  seogi_run_event_loop(&loop);

  seogi_shutdown_dbus(&state);

  return 0;
}
