#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <linux/input.h>
#include <linux/uinput.h>

#define die(str, args...) do { \
        perror(str); \
        exit(EXIT_FAILURE); \
    } while(0)

int main() {
  int fd;

  fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
  if(fd < 0) {
      die("error: open");
  }
  int ret;
  if (ioctl(fd, UI_SET_EVBIT, EV_KEY) < 0) {
    die("error: ioctl");
  }
  if (ioctl(fd, UI_SET_KEYBIT, KEY_D) < 0) {
    die("error: ioctl");
  }

  struct uinput_user_dev uidev;

  memset(&uidev, 0, sizeof(uidev));

  snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "uinput-sample");
  uidev.id.bustype = BUS_USB;
  uidev.id.vendor  = 0x1234;
  uidev.id.product = 0xfedc;
  uidev.id.version = 1;

  if (write(fd, &uidev, sizeof(uidev)) < 0) {
    die("error: write");
  }
  if (ioctl(fd, UI_DEV_CREATE) < 0) {
    die("error: ioctl");
  }

  struct input_event ev;

  memset(&ev, 0, sizeof(ev));

  ev.type = EV_KEY;
  ev.code = KEY_D;
  ev.value = 1;

  if (write(fd, &ev, sizeof(ev)) < 0) {
    die("error: write");
  }

  sleep(2);

  if (ioctl(fd, UI_DEV_DESTROY) < 0) {
    die("error: ioctl");
  }

  close(fd);
  return 0;
}
