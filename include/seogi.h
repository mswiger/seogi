#pragma once

#include <hangul.h>
#include <stdbool.h>
#include <systemd/sd-bus.h>
#include <wayland-client-core.h>
#include <xkbcommon/xkbcommon.h>

#include "key-buffer.h"

struct seogi_state {
  struct wl_display *display;  
  struct zwp_input_method_manager_v2 *input_method_manager;
  struct zwp_virtual_keyboard_manager_v1 *virtual_keyboard_manager;

  struct sd_bus *bus;

  struct wl_list seats;

  bool enabled_by_default;
  xkb_keysym_t toggle_key;
};

struct seogi_seat {
  struct wl_list link;
  struct wl_seat *wl_seat;
  struct seogi_state *state;

  const char *name;

  HangulInputContext *input_context;
  struct zwp_input_method_v2 *input_method;
  struct zwp_input_method_keyboard_grab_v2 *keyboard_grab;
  struct zwp_virtual_keyboard_v1 *virtual_keyboard;

  struct xkb_context *xkb_context;
  struct xkb_keymap *xkb_keymap;
  struct xkb_state *xkb_state;
  char *xkb_keymap_string;

  bool active;
  bool enabled;
  uint32_t serial;
  bool pending_activate;
  bool pending_deactivate;
  key_buffer_t handled_keys;
  key_buffer_t pressed_keys;
};

