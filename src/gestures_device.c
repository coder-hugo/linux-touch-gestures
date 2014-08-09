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
