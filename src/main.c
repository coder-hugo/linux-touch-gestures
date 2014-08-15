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

int uinput_fd;

static void execute_events(input_event_array_t *input_events) {
  send_events(uinput_fd, input_events);
}

int main(int argc, char *argv[]) {
  int_array_t *keys = new_int_array(4);
  keys->data[0] = KEY_LEFTCTRL;
  keys->data[1] = KEY_LEFTALT;
  keys->data[2] = KEY_UP;
  keys->data[3] = KEY_DOWN;
  uinput_fd = init_uinput(keys);
  free(keys);

  if (argc > 1) {
    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
      die("error: open");
    }
    configuration_t config;
    clean_config(&config);
    config.horz_threshold_percentage = 15;
    config.vert_threshold_percentage = 15;
    config.swipe_keys[3][UP].keys[0] = KEY_LEFTCTRL;
    config.swipe_keys[3][UP].keys[1] = KEY_LEFTALT;
    config.swipe_keys[3][UP].keys[2] = KEY_DOWN;
    config.swipe_keys[3][DOWN].keys[0] = KEY_LEFTCTRL;
    config.swipe_keys[3][DOWN].keys[1] = KEY_LEFTALT;
    config.swipe_keys[3][DOWN].keys[2] = KEY_UP;
    process_events(fd, config, &execute_events);
    close(fd);
  }
  destroy_uinput(uinput_fd);
  return 0;
}
