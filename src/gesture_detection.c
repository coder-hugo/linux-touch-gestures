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

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

#include <linux/input.h>

#include "gesture_detection.h"

typedef struct point {
  int x;
  int y;
} point_t;

typedef struct mt_slots {
  uint8_t active;
  point_t points[2];
} mt_slots_t;

typedef struct gesture_start {
  point_t point;
  uint32_t distance;
} gesture_start_t;


static int test_grab(int fd) {
  int rc;
  rc = ioctl(fd, EVIOCGRAB, (void*)1);
  if (!rc) {
    ioctl(fd, EVIOCGRAB, (void*)0);
  }
  return rc;
}

static void init_gesture(point_t slot_points[2],
                         uint8_t finger_count,
                         gesture_start_t *gesture_start) {
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
                              mt_slots_t *mt_slots) {
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

static void set_syn_event(struct input_event *syn_event) {
  memset(syn_event, 0, sizeof(struct input_event));
  syn_event->type = EV_SYN;
  syn_event->code = SYN_REPORT;
}

static void set_key_event(struct input_event *key_event, int code, int value) {
  memset(key_event, 0, sizeof(struct input_event));
  key_event->type = EV_KEY;
  key_event->code = code;
  key_event->value = value;
}

static input_event_array_t *process_syn_event(struct input_event event,
                                              configuration_t config,
                                              gesture_start_t gesture_start,
                                              point_t slot_points[2],
                                              point_t thresholds,
                                              uint8_t *finger_count) {
  input_event_array_t *result = NULL;
  if (*finger_count > 0 && event.code == SYN_REPORT) {
    direction_t direction = NONE;

    int32_t x_distance, y_distance;
    x_distance = gesture_start.point.x - slot_points[0].x;
    y_distance = gesture_start.point.y - slot_points[0].y;
    if (fabs(x_distance) > fabs(y_distance)) {
      if (x_distance > thresholds.x) {
        direction = LEFT;
      } else if (x_distance < -thresholds.x) {
        direction = RIGHT;
      }
    } else {
      if (y_distance > thresholds.y) {
        direction = UP;
      } else if (y_distance < -thresholds.y) {
        direction = DOWN;
      }
    }
    if (direction != NONE) {
      uint8_t i;
      for (i = MAX_KEYS_PER_GESTURE; i > 0; i--) {
        int key = config.swipe_keys[FINGER_TO_INDEX(*finger_count)][direction].keys[i - 1];
        if (key > 0) {
          if (!result) {
            // i is the number of keys to press
            // therefore i input_events with value 1 + 1 EV_SYN event and i input_events with value 0 + EV_SYN event are needed
            result = new_input_event_array((i + 1) * 2);
            set_syn_event(&result->data[i]);
            set_syn_event(&result->data[result->length - 1]);
          }
          // press event
          set_key_event(&result->data[i - 1], key, 1);
          // release event
          set_key_event(&result->data[result->length / 2 + i - 1], key, 0);
        }
      }
      *finger_count = 0;
    }
  }
  return result ? result : new_input_event_array(0);
}

static int32_t get_axix_threshold(int fd, int axis, uint8_t percentage) {
  struct input_absinfo absinfo;
  if (ioctl(fd, EVIOCGABS(axis), &absinfo) < 0) {
    return -1;
  }
  return (absinfo.maximum - absinfo.minimum) * percentage / 100;
}

void process_events(int fd, configuration_t config, void (*callback)(input_event_array_t*)) {
  struct input_event ev[64];
  int i, rd;
  uint8_t finger_count;
  gesture_start_t gesture_start;
  mt_slots_t mt_slots;

  point_t thresholds;
  thresholds.x = get_axix_threshold(fd, ABS_X, config.horz_threshold_percentage);
  thresholds.y = get_axix_threshold(fd, ABS_Y, config.vert_threshold_percentage);

  if (thresholds.x < 0 || thresholds.y < 0) {
    return;
  }

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
        case EV_SYN: {
            input_event_array_t *input_events = process_syn_event(ev[i], config, gesture_start, mt_slots.points, thresholds, &finger_count);
            callback(input_events);
            free(input_events);
          }
          break;
      }
    }
  }
}
