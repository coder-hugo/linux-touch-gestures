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
#include "keys.h"


char *directions[4] = { "up", "down", "left", "right" };

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

static void fill_keys_array(int (*keys_array)[MAX_KEYS_PER_GESTURE], char *keys) {
  if (keys) {
    char *ptr = strtok(keys, "+");
    uint8_t i = 0;
    while (ptr) {
      if (i >= MAX_KEYS_PER_GESTURE) {
        fprintf(stderr, "error: for each gesture only %d keystrokes are allowed\n", MAX_KEYS_PER_GESTURE);
        exit(EXIT_FAILURE);
      }
      int key_code = get_key_code(ptr);
      if (key_code < 0) {
        fprintf(stderr, "error: wrong key name '%s'\n", ptr);
        exit(EXIT_FAILURE);
      }
      (*keys_array)[i] = key_code;
      ptr = strtok(NULL, "+");
      i++;
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
  result.scroll.vert = iniparser_getboolean(ini, "scroll:vertical", false);
  result.scroll.horz = iniparser_getboolean(ini, "scroll:horizontal", false);
  result.scroll.vert_delta = (int8_t) iniparser_getint(ini, "scroll:verticaldelta", 79);
  result.scroll.horz_delta = (int8_t) iniparser_getint(ini, "scroll:horizontaldelta", 30);
  result.scroll.invert_vert = iniparser_getboolean(ini, "scroll:invertvertical", false);
  result.scroll.invert_horz = iniparser_getboolean(ini, "scroll:inverthorizontal", false);
  result.vert_threshold_percentage = iniparser_getint(ini, "thresholds:vertical", 15);
  result.horz_threshold_percentage = iniparser_getint(ini, "thresholds:horizontal", 15);
  result.zoom.enabled = iniparser_getboolean(ini, "zoom:enabled", false);
  result.zoom.delta = (uint8_t) iniparser_getint(ini, "zoom:delta", 200);

  uint8_t i, j;
  for (i = 0; i < MAX_FINGERS; i++) {
    for (j = 0; j < DIRECTIONS_COUNT; j++) {
      char ini_key[16];
      sprintf(ini_key, "%d-fingers:%s", INDEX_TO_FINGER(i), directions[j]);
      fill_keys_array(&result.swipe_keys[i][j].keys, iniparser_getstring(ini, ini_key, NULL));
    }
  }
  
  iniparser_freedict(ini);
  return result;
}
