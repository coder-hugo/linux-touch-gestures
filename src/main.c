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


int main(int argc, char *argv[]) {
  int_array *keys = new_int_array(1);
  keys->data[0] = KEY_D;
  int uinput_fd = init_uinput(*keys);
  free(keys);

//  sleep(1);
//  send_key(uinput_fd, KEY_D);
  destroy_uinput(uinput_fd);

  if (argc > 1) {
    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
      die("error: open");
    }
    print_events(fd);
    close(fd);
  }
  return 0;
}
