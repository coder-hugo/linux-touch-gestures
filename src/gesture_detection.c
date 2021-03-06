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

#define SCROLL_FINGER_COUNT 2
#define SCROLL_SLOW_DOWN_FACTOR -0.006

#define PI 3.14159265358979323846264338327
#define PI_1_2 PI / 2
#define PI_3_2 3 * PI / 2


typedef struct point {
  int x;
  int y;
} point_t;

typedef struct mt_slots {
  unsigned int active;
  point_t points[2];
  point_t last_points[2];
} mt_slots_t;

typedef struct scroll {
  struct input_event last_x_abs_event;
  struct input_event last_y_abs_event;
  double x_velocity;
  double y_velocity;
  double width;
} scroll_t;

typedef struct gesture_start {
  point_t point;
} gesture_start_t;

typedef struct scroll_thread_params {
  unsigned int delta;
  int code;
  bool invert;
  void (*callback)(input_event_array_t*);
} scroll_thread_params_t;

typedef enum gesture { NO_GESTURE, SCROLL, ZOOM, SWIPE } gesture_t;

mt_slots_t mt_slots;
gesture_start_t gesture_start;
unsigned int finger_count;
volatile scroll_t scroll;
gesture_t current_gesture;
double last_zoom_distance;
bool is_click = false;

static int test_grab(int fd) {
  int rc;
  rc = ioctl(fd, EVIOCGRAB, (void*)1);
  if (!rc) {
    ioctl(fd, EVIOCGRAB, (void*)0);
  }
  return rc;
}

static double calculate_distance(point_t p1, point_t p2) {
  int x_distance, y_distance;
  x_distance = p1.x - p2.x;
  y_distance = p1.y - p2.y;
  return sqrt((x_distance * x_distance) + (y_distance * y_distance));
}

static void reset_point(point_t *p) {
  p->x = -1;
  p->y = -1;
}

static void init_gesture() {
  reset_point(&gesture_start.point);
  current_gesture = NO_GESTURE;
  reset_point(&mt_slots.points[0]);
  reset_point(&mt_slots.points[1]);
  reset_point(&mt_slots.last_points[0]);
  reset_point(&mt_slots.last_points[1]);

  if (finger_count == SCROLL_FINGER_COUNT) {
    last_zoom_distance = -1;
    scroll.width = 0;
    scroll.x_velocity = 0;
    scroll.y_velocity = 0;
  }
}

/*
 * @return number of fingers on touch device
 */
static unsigned int process_key_event(struct input_event event) {
  unsigned int finger_count;
  if (event.value == 1 && !is_click) {
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
      case BTN_LEFT:
        is_click = true;
        finger_count = 0;
        break;
      default:
        finger_count = 0;
    }
  } else if (event.value == 0) {
    switch (event.code) {
      case BTN_TOOL_FINGER:
        finger_count = 0;
        break;
      case BTN_LEFT:
        is_click = false;
        break;
    }
  }
  return finger_count;
}

/*
 * @return velocity in distance per milliseconds
 */
static double calcualte_velocity(struct input_event event1, struct input_event event2) {
  int distance = event2.value - event1.value;
  double time_delta = 
    event2.time.tv_sec * 1000 + event2.time.tv_usec / 1000 - event1.time.tv_sec * 1000 - event1.time.tv_usec / 1000;
  return distance / time_delta;
}

static void process_abs_event(struct input_event event, point_t offsets, bool invert_horz_scroll, bool invert_vert_scroll) {
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
            scroll.x_velocity = calcualte_velocity(scroll.last_x_abs_event, event) * (invert_horz_scroll ? 1 : -1);
          }
          scroll.last_x_abs_event = event;
        }
        mt_slots.last_points[mt_slots.active].x = mt_slots.points[mt_slots.active].x;
        // store the current x position for the current mt_slot
        mt_slots.points[mt_slots.active].x = event.value - offsets.x;
        break;
      case ABS_MT_POSITION_Y:
        // if finger count matches SCROLL_FINGER_COUNT and the ABS_MT_POSITION_Y is for the first finger
        // the scroll data need to be updated
        if (mt_slots.active == 0 && finger_count == SCROLL_FINGER_COUNT) {
          // check wether a correct input event was set to scroll.last_y_abs_event
          if (scroll.last_y_abs_event.type == EV_ABS && scroll.last_y_abs_event.code == ABS_MT_POSITION_Y) {
            scroll.y_velocity = calcualte_velocity(scroll.last_y_abs_event, event) * (invert_vert_scroll ? -1 : 1);
          }
          scroll.last_y_abs_event = event;
        }
        mt_slots.last_points[mt_slots.active].y = mt_slots.points[mt_slots.active].y;
        // store the current y position for the current mt_slot
        mt_slots.points[mt_slots.active].y = event.value - offsets.y;
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

static input_event_array_t *do_scroll(double distance, int delta, int rel_code, bool invert) {
  input_event_array_t *result = NULL;
  // increment the scroll width by the current moved distance
  scroll.width += distance * (invert ? -1 : 1);
  // a scroll width of delta means scroll one "scroll-unit" therefore a scroll event
  // can be first triggered if the absolute value of scroll.width exeeded delta
  if (fabs(scroll.width) > fabs(delta)) {
    result = new_input_event_array(2);
    int width = (int)(scroll.width / delta);
    set_rel_event(&result->data[0], rel_code, width);
    set_syn_event(&result->data[1]);
    scroll.width -= width * delta;
  }
  return result;
}

static input_event_array_t *do_zoom(double distance, int delta) {
  input_event_array_t *result = NULL;
  input_event_array_t *tmp = do_scroll(distance, delta, REL_WHEEL, false);
  if (tmp) {
    result = new_input_event_array(6);
    // press CTRL
    set_key_event(&result->data[0], KEY_LEFTCTRL, 1);
    set_syn_event(&result->data[1]);
    // copy the rel_event retrieve via do_scroll
    result->data[2] = tmp->data[0];
    set_syn_event(&result->data[3]);
    // release CTRL
    set_key_event(&result->data[4], KEY_LEFTCTRL, 0);
    set_syn_event(&result->data[5]);
    free(tmp);
  }
  return result;
}

static point_t create_vector(point_t p1, point_t p2) { 
  point_t result =  {
    .x = p1.x - p2.x, 
    .y = p1.y - p2.y 
  };
  return result;
}

#define determine_gesture(scroll_enabled, vector_direction_difference) \
  /* if scrolling is enable, the finger_count matches SCROLL_FINGER_COUNT and\
     the direction of the direction vectors for both fingers is equal the current_gesture\
     will be SCROLL*/\
  if (scroll_enabled && finger_count == SCROLL_FINGER_COUNT && \
      (vector_direction_difference < PI_1_2 || vector_direction_difference > PI_3_2)) { \
    current_gesture = SCROLL; \
  /* if no scrolling and zooming is enabled or the finger_count does not match\
     SCROLL_FINGER_COUNT the current_gesture will be SWIPE*/\
  } else if (!scroll_enabled || finger_count != SCROLL_FINGER_COUNT) { \
    current_gesture = SWIPE; \
  }

static double get_vector_direction(point_t v) {
  if (v.x == 0) {
    if (v.y >= 0) {
      return 0;
    } else {
      return PI;
    }
  } else if (v.y == 0) {
    if (v.x >= 0) {
      return PI / 2;
    } else {
      return 3 * PI / 2;
    }
  } else if (v.x > 0) {
    if (v.y > 0) {
      return atan(v.x / v.y);
    } else {
      return PI / 2 + atan(-v.y / v.x);
    }
  } else {
    if (v.y < 0) {
      return PI + atan(v.x / v.y);
    } else {
      return 3 * PI / 2 + atan(v.y / -v.x);
    }
  }
}

static bool is_valid_point(point_t p) {
  return p.x > -1 && p.y > -1;
}

static bool check_mt_slots() {
  bool result = is_valid_point(mt_slots.last_points[0]) && is_valid_point(mt_slots.points[0]);
  if (result && finger_count > 1) {
    result = is_valid_point(mt_slots.last_points[1]) && is_valid_point(mt_slots.points[1]);
  }

  return result;
}

unsigned int syn_counter = 0;

static input_event_array_t *process_syn_event(struct input_event event,
                                              configuration_t config,
                                              point_t thresholds) {
  input_event_array_t *result = NULL;
  if (finger_count > 0 && event.code == SYN_REPORT) {
    if (!check_mt_slots()) {
      return new_input_event_array(0);
    } else if (!is_valid_point(gesture_start.point)) {
      gesture_start.point = mt_slots.points[0];
    }

    direction_t direction = NONE;
    double vector_direction_difference;
    if (current_gesture == NO_GESTURE) {
      double v1_direction = get_vector_direction(create_vector(mt_slots.last_points[0], mt_slots.points[0]));
      double v2_direction = get_vector_direction(create_vector(mt_slots.last_points[1], mt_slots.points[1]));
      vector_direction_difference =  fabs(v1_direction - v2_direction);
      // if zooming is enable, the finger_count matches SCROLL_FINGER_COUNT and the direction
      // vectors for both fingers are opposed to each other the current_gesture will be ZOOM
      if (config.zoom.enabled && finger_count == SCROLL_FINGER_COUNT && 
          vector_direction_difference > PI_1_2 && vector_direction_difference < PI_3_2) {
        current_gesture = ZOOM;
      }
    }

    if (current_gesture == ZOOM) {
      double finger_distance = calculate_distance(mt_slots.points[0], mt_slots.points[1]);
      if (last_zoom_distance > -1) {
        result = do_zoom(finger_distance - last_zoom_distance, config.zoom.delta);
      }
      last_zoom_distance = finger_distance;
    } else {
      int x_distance, y_distance;
      x_distance = gesture_start.point.x - mt_slots.points[0].x;
      y_distance = gesture_start.point.y - mt_slots.points[0].y;
      if (fabs(x_distance) > fabs(y_distance)) {
        if (current_gesture == NO_GESTURE) {
          determine_gesture(config.scroll.horz, vector_direction_difference);
        }

        if (current_gesture == SWIPE) {
          if (x_distance > thresholds.x) {
            direction = LEFT;
          } else if (x_distance < -thresholds.x) {
            direction = RIGHT;
          }
        } else if (current_gesture == SCROLL) {
          result = do_scroll(mt_slots.last_points[0].x - mt_slots.points[0].x, config.scroll.horz_delta, REL_HWHEEL, config.scroll.invert_horz);
        }
      } else {
        if (current_gesture == NO_GESTURE) {
          determine_gesture(config.scroll.vert, vector_direction_difference);
        }

        if (current_gesture == SWIPE) {
          if (y_distance > thresholds.y) {
            direction = UP;
          } else if (y_distance < -thresholds.y) {
            direction = DOWN;
          }
        } else if (current_gesture == SCROLL) {
          result = do_scroll(mt_slots.points[0].y - mt_slots.last_points[0].y, config.scroll.vert_delta, REL_WHEEL, config.scroll.invert_vert);
        }
      }
    }
    if (direction != NONE) {
      unsigned int i;
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

static int get_axix_threshold(int fd, int axis, unsigned int percentage) {
  struct input_absinfo absinfo;
  if (ioctl(fd, EVIOCGABS(axis), &absinfo) < 0) {
    return -1;
  }
  return (absinfo.maximum - absinfo.minimum) * percentage / 100;
}

static int get_axix_offset(int fd, int axis) {
  struct input_absinfo absinfo;
  if (ioctl(fd, EVIOCGABS(axis), &absinfo) < 0) {
    return -1;
  }
  return absinfo.minimum;
}

#define slowdown_scroll(velocity, thread_params) \
  while (velocity != 0) { \
    struct timespec tim = { \
      .tv_sec = 0, \
      .tv_nsec = 5000000 \
    };\
    nanosleep(&tim, NULL); \
    input_event_array_t *events = do_scroll(velocity * 5, thread_params->delta, thread_params->code, thread_params->invert); \
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

int process_events(int fd, configuration_t config, void (*callback)(input_event_array_t*)) {
  struct input_event ev[64];
  int i, rd;

  point_t thresholds;
  thresholds.x = get_axix_threshold(fd, ABS_X, config.horz_threshold_percentage);
  thresholds.y = get_axix_threshold(fd, ABS_Y, config.vert_threshold_percentage);

  point_t offsets;
  offsets.x = get_axix_offset(fd, ABS_X);
  offsets.y = get_axix_offset(fd, ABS_Y);

  pthread_t scroll_thread = (pthread_t) NULL;

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
      return 1;
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
          } else if (current_gesture == SCROLL && (scroll.x_velocity != 0 || scroll.y_velocity != 0)) {
            scroll_thread_params_t params = {
              .callback = callback
            };
            if (fabs(scroll.x_velocity * config.scroll.horz_delta) > fabs(scroll.y_velocity * config.scroll.vert_delta)) {
              params.delta = config.scroll.horz_delta;
              params.code = REL_HWHEEL;
              params.invert = config.scroll.invert_horz;
              scroll.y_velocity = 0;
            } else {
              params.delta = config.scroll.vert_delta;
              params.code = REL_WHEEL;
              params.invert = config.scroll.invert_vert;
              scroll.x_velocity = 0;
            }
            pthread_create(&scroll_thread, NULL, &scroll_thread_function, (void*) &params);
          }
          break;
        case EV_ABS:
            process_abs_event(ev[i], offsets, config.scroll.invert_horz, config.scroll.invert_vert);
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
  return 0;
}
