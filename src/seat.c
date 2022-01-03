#include <stdlib.h>
#include <wayland-client.h>

#include "dbus.h"
#include "seat.h"

static void handle_seat_capabilities(void *data, struct wl_seat *wl_seat, uint32_t capabilities) {
  // No-op
}

static void handle_seat_name(void *data, struct wl_seat *wl_seat, const char *name) {
  struct seogi_seat *seat = data;
  seat->name = strdup(name);
  seogi_create_dbus_seat(seat);
}

static const struct wl_seat_listener wl_seat_listener = {
  .capabilities = handle_seat_capabilities,
  .name = handle_seat_name,
};

struct seogi_seat *seogi_create_seat(struct seogi_state *state, struct wl_seat *wl_seat) {
  struct seogi_seat *seat = calloc(1, sizeof(*seat));
  seat->wl_seat = wl_seat;
  seat->state = state;
  wl_seat_add_listener(wl_seat, &wl_seat_listener, seat);
  wl_list_insert(&state->seats, &seat->link);
  return seat;
}

void seogi_toggle_seat(struct seogi_seat *seat) {
  seat->enabled = !seat->enabled;
  seogi_emit_seat_status_changed(seat);
  if (!seat->enabled) {
    hangul_ic_reset(seat->input_context);
  }
}

void seogi_enable_seat(struct seogi_seat *seat) {
  if (seat->enabled) {
    return;
  }
  seogi_toggle_seat(seat);
}

void seogi_disable_seat(struct seogi_seat *seat) {
  if (!seat->enabled) {
    return;
  }
  seogi_toggle_seat(seat);
}
