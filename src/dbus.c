#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <systemd/sd-bus.h>

#include "dbus.h"

static const char *seogi_service = "dev.swiger.Seogi";
static const char *seogi_seat_manager_path = "/dev/swiger/Seogi/SeatManager";
static const char *seogi_seat_interface = "dev.swiger.Seogi.Seat";

static int get_status_property(
  sd_bus *bus,
  const char *path,
  const char *interface,
  const char *property,
  sd_bus_message *reply,
  void *userdata,
  sd_bus_error *ret_error
) {
  char *seat_name = NULL;

  int ret = 0;
  if ((ret = sd_bus_path_decode(path, seogi_seat_manager_path, &seat_name)) < 1) {
    return ret;
  }

  struct seogi_state *state = userdata;
  struct seogi_seat *seat;
  bool found = false;
  wl_list_for_each(seat, &state->seats, link) {
    if (strcmp(seat_name, seat->name) == 0) {
      sd_bus_message_append(reply, "b", seat->enabled);
      found = true;
      break;
    }
  }
  free(seat_name);

  if (!found) {
    return EINVAL;
  }
  return 1;
}

static const sd_bus_vtable seogi_seat_vtable[] = {
  SD_BUS_VTABLE_START(0),
  SD_BUS_PROPERTY("Status", "b", get_status_property, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
  SD_BUS_VTABLE_END,
};

bool seogi_init_dbus(struct seogi_state *state) {
  int ret;
  
  if ((ret = sd_bus_open_user(&state->bus)) < 0) {
    fprintf(stderr, "sd_bus_open failed: %d\n", ret);
    return false;
  }
  if ((ret = sd_bus_request_name(state->bus, seogi_service, 0)) < 0) {
    fprintf(stderr, "sd_bus_request_name failed: %d\n", ret);
    return false;
  }
  if ((ret = sd_bus_add_object_manager(state->bus, NULL, seogi_seat_manager_path)) < 0) {
    fprintf(stderr, "sd_bus_add_object_manager failed: %d\n", ret);
    return false;
  }

  state->display = wl_display_connect(NULL);
  if (state->display == NULL) {
    fprintf(stderr, "failed to connect to Wayland display\n");
    return false;
  }

  return true;
}

void seogi_shutdown_dbus(struct seogi_state *state) {
  sd_bus_unref(state->bus);
}

bool seogi_create_dbus_seat(struct seogi_seat *seat) {
  int ret;
  
  char *seat_path = NULL;
  ret = sd_bus_path_encode(seogi_seat_manager_path, seat->name, &seat_path);
  if (ret < 0) {
    fprintf(stderr, "sd_bus_path_encode failed: %d\n", ret);
    return false;
  }

  struct seogi_state *state = seat->state;
  ret = sd_bus_add_object_vtable(state->bus, NULL, seat_path, seogi_seat_interface, seogi_seat_vtable, state);
  free(seat_path);
  if (ret < 0) {
    fprintf(stderr, "sd_bus_add_object_vtable failed: %d\n", ret);
  }

  return true;
}

bool seogi_emit_seat_status_changed(struct seogi_seat *seat) {
  int ret;

  char *seat_path = NULL;
  if ((ret = sd_bus_path_encode(seogi_seat_manager_path, seat->name, &seat_path)) < 0) {
    fprintf(stderr, "sd_bus_path_encode failed: %d\n", ret);
    return false;
  }

  sd_bus_emit_properties_changed(seat->state->bus, seat_path, seogi_seat_interface, "Status", NULL);

  free(seat_path);
  return true;
}
