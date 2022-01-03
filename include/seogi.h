#pragma once

#include <stdbool.h>
#include <systemd/sd-bus.h>
#include <wayland-client-core.h>
#include <xkbcommon/xkbcommon.h>

struct seogi_state {
  struct wl_display *display;  
  struct zwp_input_method_manager_v2 *input_method_manager;
  struct zwp_virtual_keyboard_manager_v1 *virtual_keyboard_manager;

  struct sd_bus *bus;

  struct wl_list seats;

  bool enabled_by_default;
  xkb_keysym_t toggle_key;
};
