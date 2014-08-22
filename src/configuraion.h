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

#ifndef CONFIGURATION_H_
#define CONFIGURATION_H_

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "int_array.h"

#define MAX_FINGERS           5
#define DIRECTIONS_COUNT      4
#define MAX_KEYS_PER_GESTURE  5

typedef struct keys_array {
  int keys[MAX_KEYS_PER_GESTURE];
} keys_array_t;

typedef struct configuration {
  char *touch_device_path;
  struct scroll_options {
    bool vert;
    bool horz;
    int8_t vert_delta;
    int8_t horz_delta;
  } scroll;
  struct zoom_options {
    bool enabled;
    uint8_t delta;
  } zoom;
  uint8_t vert_threshold_percentage;
  uint8_t horz_threshold_percentage;
  keys_array_t swipe_keys[MAX_FINGERS][DIRECTIONS_COUNT];
} configuration_t;

typedef enum direction { UP, DOWN, LEFT, RIGHT, NONE } direction_t;

configuration_t read_config(const char *filename);

#define FINGER_TO_INDEX(finger) (finger - 1)
#define INDEX_TO_FINGER(index) (index + 1)

#endif // CONFIGURATION_H_
