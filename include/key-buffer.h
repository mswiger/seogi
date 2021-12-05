#pragma once

#include <stdbool.h>
#include <xkbcommon/xkbcommon.h>

#define KEY_BUFFER_SIZE 64
#define KEY_BUFFER_ITER_LENGTH sizeof(key_buffer_t) / sizeof(xkb_keycode_t)

typedef xkb_keycode_t key_buffer_t[KEY_BUFFER_SIZE];

bool key_buffer_contains(key_buffer_t buffer, xkb_keycode_t key);
void key_buffer_insert(key_buffer_t buffer, xkb_keycode_t key);
bool key_buffer_remove(key_buffer_t buffer, xkb_keycode_t key);
void key_buffer_clear(key_buffer_t buffer);
