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
#include <stdbool.h>
#include <iniparser.h>

#include "configuraion.h"
#include "common.h"

static void clean_config(configuration_t *config) {
  int32_t i, j, k;
  for (i = 0; i < MAX_FINGERS; i++) {
    for (j = 0; j < DIRECTIONS_COUNT; j++) {
      for (k = 0; k < MAX_KEYS_PER_GESTURE; k++) {
        config->swipe_keys[i][j].keys[k] = -1;
      }
    }
  }
}

configuration_t read_config(const char *filename) {
  configuration_t result;
  clean_config(&result);
  dictionary *ini = iniparser_load(filename);
  char *touch_device_path = iniparser_getstring(ini, "general:touchdevice", NULL);
  if (touch_device_path) {
    if (result.touch_device_path = malloc(strlen(touch_device_path) + 1)) {
      strcpy(result.touch_device_path, touch_device_path);
    }
  } else {
    errno = EINVAL;
    die("error: no touch device defined");
  }
  result.vert_scroll = iniparser_getboolean(ini, "scroll:vertical", false);
  result.horz_scroll = iniparser_getboolean(ini, "scroll:horizontal", false);
  result.vert_threshold_percentage = iniparser_getint(ini, "thresholds:vertical", 15),
  result.horz_threshold_percentage = iniparser_getint(ini, "thresholds:horizontal", 15),
  iniparser_freedict(ini);
  return result;
}
