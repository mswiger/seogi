#include <errno.h>
#include <stdio.h>
#include <wayland-client.h>
#include "event-loop.h"

void seogi_init_event_loop(struct seogi_event_loop *loop, struct sd_bus *bus, struct wl_display *display) {
  loop->fds[SEOGI_EVENT_DBUS] = (struct pollfd) {
    .fd = sd_bus_get_fd(bus),
    .events = POLLIN,
  };

  loop->fds[SEOGI_EVENT_WAYLAND] = (struct pollfd) {
    .fd = wl_display_get_fd(display),
    .events = POLLIN,
  };

  loop->bus = bus;
  loop->display = display;
}

int seogi_run_event_loop(struct seogi_event_loop *loop) {
  loop->running = true;

  int ret = 0;

  // Handle unprocessed messages queued up by sync sd_bus methods
  do {
    ret = sd_bus_process(loop->bus, NULL);
  } while (ret > 0);

  if (ret < 0) {
    return ret;
  }

  while (loop->running) {
    // Flush Wayland events generated while handling non-Wayland events
    do {
      ret = wl_display_dispatch_pending(loop->display);
      wl_display_flush(loop->display);
    } while (ret > 0);

    if (ret < 0) {
      fprintf(stderr, "failed to dispatch pending Wayland events\n");
      break;
    }

    // Same for D-Bus
    sd_bus_flush(loop->bus);

    ret = poll(loop->fds, SEOGI_EVENT_COUNT, -1);
    if (ret < 0) {
      fprintf(stderr, "failed to poll(): %s\n", strerror(errno));
      break;
    }

    for (size_t i = 0; i < SEOGI_EVENT_COUNT; i++) {
      if (loop->fds[i].revents & POLLHUP) {
        loop->running = false;
        break;
      }
      if (loop->fds[i].revents & POLLERR) {
        fprintf(stderr, "failed to poll() socket %zu\n", i);
        ret = -1;
        break;
      }
    }

    if (loop->fds[SEOGI_EVENT_DBUS].revents & POLLIN) {
      do {
        ret = sd_bus_process(loop->bus, NULL);
      } while (ret > 0);

      if (ret < 0) {
        fprintf(stderr, "failed to process D-Bus: %s\n", strerror(-ret));
        break;
      }
    }

    if (loop->fds[SEOGI_EVENT_WAYLAND].revents & POLLIN) {
      ret = wl_display_dispatch(loop->display);
      if (ret < 0) {
        fprintf(stderr, "failed to read Wayland evetns\n");
        break;
      }
    }

    if (loop->fds[SEOGI_EVENT_WAYLAND].revents & POLLOUT) {
      ret = wl_display_flush(loop->display);
      if (ret < 0) {
        fprintf(stderr, "failed to flush Wayland events\n");
      }
    }
  }

  return ret;
}
