#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>


#include <linux/input.h>

#include "common.h"
#include "gestures_device.h"


int finger_count = 0;

typedef struct t_point {
  int x;
  int y;
} point;

point start_point;

int active_slot;

static int test_grab(int fd) {
  int rc;
  rc = ioctl(fd, EVIOCGRAB, (void*)1);
  if (!rc) {
    ioctl(fd, EVIOCGRAB, (void*)0);
  }
  return rc;
}

void set_start_point(int fd) {
  struct input_mt_request_layout {
    uint32_t code;
    int32_t values[2];
  } data;
  data.code = ABS_MT_POSITION_X;
  if (ioctl(fd, EVIOCGMTSLOTS(sizeof(data)), &data) < 0) {
    return;
  }
  start_point.x = data.values[0];
  data.code = ABS_MT_POSITION_Y;
  if (ioctl(fd, EVIOCGMTSLOTS(sizeof(data)), &data) < 0) {
    return;
  }
  start_point.y = data.values[0];
}

static void print_events(int fd) {
  struct input_event ev[64];
  int i, rd;

  while (1) {
    rd = read(fd, ev, sizeof(struct input_event) * 64);

    if (rd < (int) sizeof(struct input_event)) {
      printf("expected %d bytes, got %d\n", (int) sizeof(struct input_event), rd);
      return;
    }

    for (i = 0; i < rd / sizeof(struct input_event); i++) {
      if (ev[i].type == EV_KEY) {
        if (ev[i].value == 1) {
          switch (ev[i].code) {
            case BTN_TOOL_FINGER:
              finger_count = 1;
              set_start_point(fd);
              break;
            case BTN_TOOL_DOUBLETAP:
              finger_count = 2;
              set_start_point(fd);
              break;
            case BTN_TOOL_TRIPLETAP:
              finger_count = 3;
              set_start_point(fd);
              break;
            case BTN_TOOL_QUADTAP:
              finger_count = 4;
              set_start_point(fd);
              break;
            case BTN_TOOL_QUINTTAP:
              finger_count = 5;
              set_start_point(fd);
              break;
            default:
              finger_count = 0;
          }
        } else if (ev[i].value == 0 && BTN_TOOL_FINGER) {
          finger_count = 0;
        }
      }

      if (finger_count > 0 && ev[i].type == EV_ABS) {
        if (ev[i].code == ABS_MT_SLOT) {
          active_slot = ev[i].value;
        } else if (active_slot == 0) {
          int difference;
          switch (ev[i].code) {
            case ABS_MT_POSITION_X:
              difference = start_point.x - ev[i].value;
              if (difference > 500) {
                printf("%d fingers left\n", finger_count);
                finger_count = 0;
              } else if (difference < -500) {
                printf("%d fingers right\n", finger_count);
                finger_count = 0;
              }
              break;
            case ABS_MT_POSITION_Y:
              difference = start_point.y - ev[i].value;
              if (difference > 300) {
                printf("%d fingers up\n", finger_count);
                finger_count = 0;
              } else if (difference < -300) {
                printf("%d fingers down\n", finger_count);
                finger_count = 0;
              }
              break;
          }
        }
      }
    }
  }
}

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
    if (!test_grab(fd)) {
      print_events(fd);
    }
    close(fd);
  }
  return 0;
}
