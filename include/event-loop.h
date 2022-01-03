#pragma once

#include <poll.h>
#include <stdbool.h>
#include <systemd/sd-bus.h>
#include <wayland-client-core.h>

enum seogi_event {
  SEOGI_EVENT_DBUS,
  SEOGI_EVENT_WAYLAND,
  SEOGI_EVENT_COUNT, // keep last
};

struct seogi_event_loop {
  struct pollfd fds[SEOGI_EVENT_COUNT];
  sd_bus *bus;
  struct wl_display *display;

  bool running;
};

void seogi_init_event_loop(struct seogi_event_loop *loop, struct sd_bus *bus, struct wl_display *display);
int seogi_run_event_loop(struct seogi_event_loop *loop);
