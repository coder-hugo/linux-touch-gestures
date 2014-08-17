/*
 * The MIT License
 *
 * Copyright 2014 Robin Müller.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <linux/input.h>

#include "common.h"
#include "gestures_device.h"
#include "gesture_detection.h"

int uinput_fd, touch_device_fd;

static void execute_events(input_event_array_t *input_events) {
  send_events(uinput_fd, input_events);
}

static int_array_t *get_keys_array(configuration_t config) {
  uint8_t i, j, k;
  uint8_t keys_count = 0;
  int_array_t *keys = new_int_array(MAX_FINGERS * DIRECTIONS_COUNT * MAX_KEYS_PER_GESTURE);
  for (i = 0; i < MAX_FINGERS; i++) {
    for (j = 0; j < DIRECTIONS_COUNT; j++) {
      for (k = 0; k < MAX_KEYS_PER_GESTURE; k++) {
        if (config.swipe_keys[i][j].keys[k] != -1) {
          keys->data[keys_count] = config.swipe_keys[i][j].keys[k];
          keys_count++;
        }
      }
    }
  }
  keys->length = keys_count;
  return keys;
}

int main(int argc, char *argv[]) {
  if (argc > 1) {
    configuration_t config = read_config(argv[1]);
    int_array_t *keys = get_keys_array(config);
    uinput_fd = init_uinput(keys);
    free(keys);

    touch_device_fd = open(config.touch_device_path, O_RDONLY);
    if (touch_device_fd < 0) {
      die("error: open");
    }
    process_events(touch_device_fd, config, &execute_events);

    close(touch_device_fd);
    destroy_uinput(uinput_fd);
  }
  return 0;
}
