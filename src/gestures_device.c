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

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>

#include <linux/input.h>
#include <linux/uinput.h>

#include "common.h"
#include "gestures_device.h"

int init_uinput(int_array_t *keys) {
  int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
  if(fd < 0) {
      die("error: open");
  }

  ioctl(fd, UI_SET_EVBIT, EV_KEY);
  ioctl(fd, UI_SET_EVBIT, EV_SYN);
  ioctl(fd, UI_SET_EVBIT, EV_REL);
  
  ioctl(fd, UI_SET_RELBIT, REL_WHEEL);
  ioctl(fd, UI_SET_RELBIT, REL_HWHEEL);

  int i;
  for (i = 0; i < keys->length; i++) {
    ioctl(fd, UI_SET_KEYBIT, keys->data[i]);
  }

  struct uinput_user_dev uidev;

  memset(&uidev, 0, sizeof(uidev));

  snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "uinput-touch-gestures");
  uidev.id.bustype = BUS_USB;
  uidev.id.vendor  = 0x1;
  uidev.id.product = 0x1;
  uidev.id.version = 1;

  if (write(fd, &uidev, sizeof(uidev)) < 0) {
    die("error: write");
  }
  if (ioctl(fd, UI_DEV_CREATE) < 0) {
    die("error: ioctl");
  }
  return fd;
}

int destroy_uinput(int fd) {
  if (ioctl(fd, UI_DEV_DESTROY) < 0) {
    die("error: ioctl");
  }

  close(fd);
}

void send_events(int fd, input_event_array_t *input_events) {
  uint8_t i;
  if (input_events->length > 0) {
    for (i = 0; i < input_events->length; i++) {
      if (write(fd, &input_events->data[i], sizeof(struct input_event)) < 0) {
        die("error: write");
      }
    }
  }
}
