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
#include <time.h>
#include <pthread.h>

#include <linux/input.h>

#include "gesture_detection.h"

// FIXME use 2 for SCROLL_FINGER_COUNT as soon as possible
#define SCROLL_FINGER_COUNT 4
#define SCROLL_SLOW_DOWN_FACTOR -0.006

typedef struct point {
  int x;
  int y;
} point_t;

typedef struct mt_slots {
  uint8_t active;
  point_t points[2];
} mt_slots_t;

typedef struct scroll {
  point_t last_point;
  struct input_event last_x_abs_event;
  struct input_event last_y_abs_event;
  double x_velocity;
  double y_velocity;
  double width;
} scroll_t;

typedef struct gesture_start {
  point_t point;
  uint32_t distance;
} gesture_start_t;

typedef struct scroll_thread_params {
  uint8_t delta;
  int code;
  void (*callback)(input_event_array_t*);
} scroll_thread_params_t;

mt_slots_t mt_slots;
gesture_start_t gesture_start;
uint8_t finger_count;
volatile scroll_t scroll;

static int test_grab(int fd) {
  int rc;
  rc = ioctl(fd, EVIOCGRAB, (void*)1);
  if (!rc) {
    ioctl(fd, EVIOCGRAB, (void*)0);
  }
  return rc;
}

static void init_gesture() {
  int32_t x_distance, y_distance;
  gesture_start.point.x = mt_slots.points[0].x;
  gesture_start.point.y = mt_slots.points[0].y;

  if (finger_count == SCROLL_FINGER_COUNT) {
    x_distance = mt_slots.points[0].x - mt_slots.points[1].x;
    y_distance = mt_slots.points[0].y - mt_slots.points[1].y;
    gesture_start.distance = (uint32_t) sqrt((x_distance * x_distance) + (y_distance * y_distance));
    scroll.width = 0;
    scroll.last_point = mt_slots.points[0];
    scroll.x_velocity = 0;
    scroll.y_velocity = 0;
  }
}

/*
 * @return number of fingers on touch device
 */
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

/*
 * @return velocity in distance per milliseconds
 */
static double calcualte_velocity(struct input_event event1, struct input_event event2) {
  int32_t distance = event2.value - event1.value;
  double time_delta = 
    event2.time.tv_sec * 1000 + event2.time.tv_usec / 1000 - event1.time.tv_sec * 1000 - event1.time.tv_usec / 1000;
  return distance / time_delta;
}

static void process_abs_event(struct input_event event) {
  if (event.code == ABS_MT_SLOT) {
    // store the current mt_slot
    mt_slots.active = event.value;
  } else if (mt_slots.active < 2) {
    switch (event.code) {
      case ABS_MT_POSITION_X:
        // if finger count matches SCROLL_FINGER_COUNT and the ABS_MT_POSITION_X is for the first finger
        // the scroll data need to be updated
        if (mt_slots.active == 0 && finger_count == SCROLL_FINGER_COUNT) {
          // check wether a correct input event was set to scroll.last_x_abs_event
          if (scroll.last_x_abs_event.type == EV_ABS && scroll.last_x_abs_event.code == ABS_MT_POSITION_X) {
            // invert the velocity to scroll to the correct direction as a positive x direction
            // on the touchpad mean scroll left (negative scroll direction)
            scroll.x_velocity = -calcualte_velocity(scroll.last_x_abs_event, event);
          }
          scroll.last_x_abs_event = event;
          scroll.last_point.x = mt_slots.points[0].x;
        }
        // store the current x position for the current mt_slot
        mt_slots.points[mt_slots.active].x = event.value;
        break;
      case ABS_MT_POSITION_Y:
        // if finger count matches SCROLL_FINGER_COUNT and the ABS_MT_POSITION_Y is for the first finger
        // the scroll data need to be updated
        if (mt_slots.active == 0 && finger_count == SCROLL_FINGER_COUNT) {
          // check wether a correct input event was set to scroll.last_y_abs_event
          if (scroll.last_y_abs_event.type == EV_ABS && scroll.last_y_abs_event.code == ABS_MT_POSITION_Y) {
            scroll.y_velocity = calcualte_velocity(scroll.last_y_abs_event, event);
          }
          scroll.last_y_abs_event = event;
          scroll.last_point.y = mt_slots.points[0].y;
        }
        // store the current y position for the current mt_slot
        mt_slots.points[mt_slots.active].y = event.value;
        break;
    }
  }
}

static void set_input_event(struct input_event *input_event, int type, int code, int value) {
  memset(input_event, 0, sizeof(struct input_event));
  input_event->type = type;
  input_event->code = code;
  input_event->value = value;
}

#define set_syn_event(syn_event) set_input_event(syn_event, EV_SYN, SYN_REPORT, 0)
#define set_key_event(key_event, code, value) set_input_event(key_event, EV_KEY, code, value)
#define set_rel_event(rel_event, code, value) set_input_event(rel_event, EV_REL, code, value)

static input_event_array_t *do_scroll(double distance, int32_t delta, int rel_code) {
  input_event_array_t *result = NULL;
  // increment the scroll width by the current moved distance
  scroll.width += distance;
  // a scroll width of delta means scroll one "scroll-unit" therefore a scroll event
  // can be first triggered if the absolute value of scroll.width exeeded delta
  if (fabs(scroll.width) > delta) {
    result = new_input_event_array(2);
    int width = (int)(scroll.width / delta);
    set_rel_event(&result->data[0], rel_code, width);
    set_syn_event(&result->data[1]);
    scroll.width -= width * delta;
  }
  return result;
}

static input_event_array_t *process_syn_event(struct input_event event,
                                              configuration_t config,
                                              point_t thresholds) {
  input_event_array_t *result = NULL;
  if (finger_count > 0 && event.code == SYN_REPORT) {
    direction_t direction = NONE;

    int32_t x_distance, y_distance;
    x_distance = gesture_start.point.x - mt_slots.points[0].x;
    y_distance = gesture_start.point.y - mt_slots.points[0].y;
    if (fabs(x_distance) > fabs(y_distance)) {
      // only check for the finger_count LEFT and RIGHT gestures if horizontal scrolling is
      // disabled and the the finger_count doesn't match SCROLL_FINGER_COUNT
      if (!(config.scroll.horz && finger_count == SCROLL_FINGER_COUNT)) {
        if (x_distance > thresholds.x) {
          direction = LEFT;
        } else if (x_distance < -thresholds.x) {
          direction = RIGHT;
        }
      } else {
        result = do_scroll(scroll.last_point.x - mt_slots.points[0].x, config.scroll.horz_delta, REL_HWHEEL);
      }
    } else {
      // only check for the finger_count UP and DOWN gestures if horizontal scrolling is
      // disabled and the the finger_count doesn't match SCROLL_FINGER_COUNT
      if (!(config.scroll.vert && finger_count == SCROLL_FINGER_COUNT)) {
        if (y_distance > thresholds.y) {
          direction = UP;
        } else if (y_distance < -thresholds.y) {
          direction = DOWN;
        }
      } else {
        result = do_scroll(mt_slots.points[0].y - scroll.last_point.y, config.scroll.vert_delta, REL_WHEEL);
      }
    }
    if (direction != NONE) {
      uint8_t i;
      for (i = MAX_KEYS_PER_GESTURE; i > 0; i--) {
        int key = config.swipe_keys[FINGER_TO_INDEX(finger_count)][direction].keys[i - 1];
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
      finger_count = 0;
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

#define slowdown_scroll(velocity, thread_params) \
  while (velocity != 0) { \
    struct timespec tim = { \
      .tv_sec = 0, \
      .tv_nsec = 5000000 \
    };\
    nanosleep(&tim, NULL); \
    input_event_array_t *events = do_scroll(velocity * 5, thread_params->delta, thread_params->code); \
    if (events) { \
      thread_params->callback(events); \
    } \
    free(events); \
    double new_velocity = SCROLL_SLOW_DOWN_FACTOR * 5 + fabs(velocity); \
    if (new_velocity < 0) { \
      new_velocity = 0; \
    } \
    if (velocity > 0) { \
      velocity = new_velocity; \
    } else { \
      velocity = -new_velocity; \
    } \
  }

static void *scroll_thread_function(void *val) {
  scroll_thread_params_t *params = ((scroll_thread_params_t*)val);
  if (params->code == REL_WHEEL) {
    slowdown_scroll(scroll.y_velocity, params);
  } else if (params->code == REL_HWHEEL) {
    slowdown_scroll(scroll.x_velocity, params);
  }
  return NULL;
}

void process_events(int fd, configuration_t config, void (*callback)(input_event_array_t*)) {
  struct input_event ev[64];
  int i, rd;

  point_t thresholds;
  thresholds.x = get_axix_threshold(fd, ABS_X, config.horz_threshold_percentage);
  thresholds.y = get_axix_threshold(fd, ABS_Y, config.vert_threshold_percentage);

  pthread_t scroll_thread = NULL;

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
            if (scroll_thread) {
              pthread_cancel(scroll_thread);
            }
            init_gesture();
          } else if (scroll.x_velocity != 0 || scroll.y_velocity != 0) {
            scroll_thread_params_t params = {
              .callback = callback
            };
            if (fabs(scroll.x_velocity * config.scroll.horz_delta) > fabs(scroll.y_velocity * config.scroll.vert_delta)) {
              params.delta = config.scroll.horz_delta;
              params.code = REL_HWHEEL;
              scroll.y_velocity = 0;
            } else {
              params.delta = config.scroll.vert_delta;
              params.code = REL_WHEEL;
              scroll.x_velocity = 0;
            }
            pthread_create(&scroll_thread, NULL, &scroll_thread_function, (void*) &params);
          }
          break;
        case EV_ABS:
          process_abs_event(ev[i]);
          break;
        case EV_SYN: {
            input_event_array_t *input_events = process_syn_event(ev[i], config, thresholds);
            callback(input_events);
            free(input_events);
          }
          break;
      }
    }
  }
}
