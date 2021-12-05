#include <string.h>

#include "key-buffer.h"

bool key_buffer_contains(key_buffer_t buffer, xkb_keycode_t key) {
  for (size_t i = 0; i < KEY_BUFFER_ITER_LENGTH; i++) {
    if (buffer[i] == key) {
      return true;
    }
  }
  return false;
}

void key_buffer_insert(key_buffer_t buffer, xkb_keycode_t key) {
  for (size_t i = 0; i < KEY_BUFFER_ITER_LENGTH; i++) {
    if (buffer[i] == 0) {
      buffer[i] = key;
      return;
    }
  }
}

bool key_buffer_remove(key_buffer_t buffer, xkb_keycode_t key) {
  for (size_t i = 0; i < KEY_BUFFER_ITER_LENGTH; i++) {
    if (buffer[i] == key) {
      buffer[i] = 0;
      return true;
    }
  }
  return false;
}

void key_buffer_clear(key_buffer_t buffer) {
  memset(buffer, 0, KEY_BUFFER_SIZE);
}
