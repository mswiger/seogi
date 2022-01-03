#pragma once

#include "seat.h"
#include "seogi.h"

bool seogi_init_dbus(struct seogi_state *state);
void seogi_shutdown_dbus(struct seogi_state *state);

bool seogi_create_dbus_seat(struct seogi_seat *seat);
bool seogi_emit_seat_status_changed(struct seogi_seat *seat);

