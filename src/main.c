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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>

#include <linux/input.h>

#include "common.h"
#include "gestures_device.h"
#include "gesture_detection.h"


#define DEV_INPUT_EVENT "/dev/input"
#define EVENT_DEV_NAME "event"

#define BITS_PER_LONG (sizeof(long) * 8)
#define NBITS(x) ((((x)-1)/BITS_PER_LONG)+1)
#define OFF(x)  ((x)%BITS_PER_LONG)
#define BIT(x)  (1UL<<OFF(x))
#define LONG(x) ((x)/BITS_PER_LONG)
#define test_bit(bit, array) ((array[LONG(bit)] >> OFF(bit)) & 1)

int uinput_fd, touch_device_fd;

static void execute_events(input_event_array_t *input_events) {
  send_events(uinput_fd, input_events);
}

static int_array_t *get_keys_array(configuration_t config) {
  unsigned int i, j, k;
  unsigned int keys_count = 0;
  int_array_t *keys = new_int_array(MAX_FINGERS * DIRECTIONS_COUNT * MAX_KEYS_PER_GESTURE);
  for (i = 0; i < MAX_FINGERS; i++) {
    for (j = 0; j < DIRECTIONS_COUNT; j++) {
      for (k = 0; k < MAX_KEYS_PER_GESTURE; k++) {
        if (config.swipe_keys[i][j].keys[k] != -1) {
          keys->data[keys_count] = config.swipe_keys[i][j].keys[k];
          keys_count++;
        }
      }
    }
  }
  keys->length = keys_count;
  return keys;
}

static int is_event_device(const struct dirent *dir) {
  return strncmp(EVENT_DEV_NAME, dir->d_name, 5) == 0;
}

static bool check_device(char *device_name) {
  char filename[64];
  int fd = -1;
  char name[256] = "???";
  unsigned long bit[NBITS(KEY_MAX)];

  snprintf(filename, sizeof(filename), "%s/%s", DEV_INPUT_EVENT, device_name);
  fd = open(filename, O_RDONLY);
  if (fd < 0) {
    return false;
  }

  memset(bit, 0, sizeof(bit));
  ioctl(fd, EVIOCGBIT(EV_KEY, KEY_MAX), bit);

  ioctl(fd, EVIOCGNAME(sizeof(name)), name);
  close(fd);

  if (test_bit(BTN_TOOL_QUINTTAP, bit)) {
    printf("Found multi-touch input device: %s\n", name);
    return true;
  }

  return false;
}

static char* scan_devices(void) {
  struct dirent **namelist;

  int devnum = -1;
  int ndev = scandir(DEV_INPUT_EVENT, &namelist, is_event_device, alphasort);
  if (ndev <= 0) {
    return NULL;
  }

  for (int i = 0; i < ndev; i++) {
    if (devnum == -1 && check_device(namelist[i]->d_name)) {
      sscanf(namelist[i]->d_name, "event%i", &devnum);
    }
    free(namelist[i]);
  }

  if (devnum == -1) {
    return NULL;
  }

  char *filename = (char*) malloc(64 * sizeof(char));
  sprintf(filename, "%s/%s%i", DEV_INPUT_EVENT, EVENT_DEV_NAME, devnum);
  return filename;
}

static int open_touch_device(configuration_t config, int retry) {
  if (config.touch_device_path) {
    printf("Looking for input device: %s (Attempt %i/%i)\n", config.touch_device_path, retry + 1, config.retries + 1);
    return open(config.touch_device_path, O_RDONLY);
  } else {
    printf("Looking for multi-touch input device: (Attempt %i/%i)\n", retry + 1, config.retries + 1);
    char *filename = scan_devices();
    if (filename) {
      return open(filename, O_RDONLY);
    } else {
      return -1;
    }
  }
}

int main(int argc, char *argv[]) {
  if (argc > 1) {
    configuration_t config = read_config(argv[1]);
    int_array_t *keys = get_keys_array(config);
    uinput_fd = init_uinput(keys);
    free(keys);

    int retry = 0;
    while (retry < config.retries + 1) {
      touch_device_fd = open_touch_device(config, retry);
      if (touch_device_fd > 0) {
        break;
      }
      sleep(config.retry_delay);
      retry++;
    }
    if (touch_device_fd < 0) {
      die("Failed to open input device");
    }
    printf("Opened input device\n");
    process_events(touch_device_fd, config, &execute_events);

    close(touch_device_fd);
    destroy_uinput(uinput_fd);
  }
  return 0;
}
