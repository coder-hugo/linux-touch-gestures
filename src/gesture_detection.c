#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>

#include <linux/input.h>

#include "gesture_detection.h"

struct point_t {
  int x;
  int y;
};

struct mt_slots_t {
  uint8_t active;
  struct point_t points[2];
};

struct gesture_start_t {
  struct point_t point;
  uint32_t distance;
};

static int test_grab(int fd) {
  int rc;
  rc = ioctl(fd, EVIOCGRAB, (void*)1);
  if (!rc) {
    ioctl(fd, EVIOCGRAB, (void*)0);
  }
  return rc;
}

static void init_gesture(struct point_t slot_points[2],
                         uint8_t finger_count,
                         struct gesture_start_t *gesture_start) {
  int32_t x_distance, y_distance;
  gesture_start->point.x = slot_points[0].x;
  gesture_start->point.y = slot_points[0].y;

  if (finger_count == 2) {
    x_distance = slot_points[0].x - slot_points[1].x;
    y_distance = slot_points[0].y - slot_points[1].y;
    gesture_start->distance = (uint32_t) sqrt((x_distance * x_distance) + (y_distance * y_distance));
  }
}

static uint8_t process_key_event(struct input_event event) {
  uint8_t finger_count;
  if (event.value == 1) {
    switch (event.code) {
      case BTN_TOOL_FINGER:
        finger_count = 1;
        break;
      case BTN_TOOL_DOUBLETAP:
        finger_count = 2;
        break;
      case BTN_TOOL_TRIPLETAP:
        finger_count = 3;
        break;
      case BTN_TOOL_QUADTAP:
        finger_count = 4;
        break;
      case BTN_TOOL_QUINTTAP:
        finger_count = 5;
        break;
      default:
        finger_count = 0;
    }
  } else if (event.value == 0 && event.code == BTN_TOOL_FINGER) {
    finger_count = 0;
  }
  return finger_count;
}

static void process_abs_event(struct input_event event,
                              struct mt_slots_t *mt_slots) {
  if (event.code == ABS_MT_SLOT) {
    mt_slots->active = event.value;
  } else if (mt_slots->active < 2) {
    switch (event.code) {
      case ABS_MT_POSITION_X:
        mt_slots->points[mt_slots->active].x = event.value;
        break;
      case ABS_MT_POSITION_Y:
        mt_slots->points[mt_slots->active].y = event.value;
        break;
    }
  }
}

static void process_syn_event(struct input_event event,
                              struct gesture_start_t gesture_start,
                              struct point_t slot_points[2],
                              uint8_t *finger_count) {
  if (*finger_count > 0 && event.code == SYN_REPORT) {
    int32_t x_distance, y_distance;
    x_distance = gesture_start.point.x - slot_points[0].x;
    y_distance = gesture_start.point.y - slot_points[0].y;
    if (fabs(x_distance) > fabs(y_distance)) {
      if (x_distance > 500) {
        printf("%d fingers left\n", *finger_count);
        *finger_count = 0;
      } else if (x_distance < -500) {
        printf("%d fingers right\n", *finger_count);
        *finger_count = 0;
      }
    } else {
      if (y_distance > 300) {
        printf("%d fingers up\n", *finger_count);
        *finger_count = 0;
      } else if (y_distance < -300) {
        printf("%d fingers down\n", *finger_count);
        *finger_count = 0;
      }
    }
  }
}

void print_events(int fd) {
  struct input_event ev[64];
  int i, rd;
  uint8_t finger_count;
  struct gesture_start_t gesture_start;
  struct mt_slots_t mt_slots;

  if (test_grab(fd) < 0) {
    return;
  }

  while (1) {
    rd = read(fd, ev, sizeof(struct input_event) * 64);

    if (rd < (int) sizeof(struct input_event)) {
      printf("expected %d bytes, got %d\n", (int) sizeof(struct input_event), rd);
      return;
    }

    for (i = 0; i < rd / sizeof(struct input_event); i++) {
      switch(ev[i].type) {
        case EV_KEY:
          finger_count = process_key_event(ev[i]);
          if (finger_count > 0) {
            init_gesture(mt_slots.points, finger_count, &gesture_start);
          }
          break;
        case EV_ABS:
          process_abs_event(ev[i], &mt_slots);
          break;
        case EV_SYN:
          process_syn_event(ev[i], gesture_start, mt_slots.points, &finger_count);
          break;
      }
    }
  }
}
