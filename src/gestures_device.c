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

#include <linux/input.h>
#include <linux/uinput.h>

#include "common.h"
#include "gestures_device.h"

int init_uinput(int_array keys) {
  int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
  if(fd < 0) {
      die("error: open");
  }

  ioctl(fd, UI_SET_EVBIT, EV_KEY);
  ioctl(fd, UI_SET_EVBIT, EV_SYN);
  ioctl(fd, UI_SET_EVBIT, EV_REL);
  
  ioctl(fd, UI_SET_RELBIT, REL_WHEEL);
  
  ioctl(fd, UI_SET_KEYBIT, BTN_LEFT);
  ioctl(fd, UI_SET_KEYBIT, BTN_RIGHT);
  ioctl(fd, UI_SET_KEYBIT, BTN_MIDDLE);

  int i;
  for (i = 0; i < keys.length; i++) {
    ioctl(fd, UI_SET_KEYBIT, keys.data[i]);
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

void send_key(int fd, int key) {
  struct input_event key_ev, syn_ev;

  memset(&key_ev, 0, sizeof(struct input_event));
  key_ev.type = EV_KEY;
  key_ev.code = key;
  // press the key
  key_ev.value = 1;
  if (write(fd, &key_ev, sizeof(struct input_event)) < 0) {
    die("error: write");
  }

  memset(&syn_ev, 0, sizeof(struct input_event));
  syn_ev.type = EV_SYN;
  syn_ev.code = 0;
  syn_ev.value = 0;
  if (write(fd, &syn_ev, sizeof(struct input_event)) < 0) {
    die("error: write");
  }

  // release the key
  key_ev.value = 0;
  if (write(fd, &key_ev, sizeof(struct input_event)) < 0) {
    die("error: write");
  }
  if (write(fd, &syn_ev, sizeof(struct input_event)) < 0) {
    die("error: write");
  }
}
