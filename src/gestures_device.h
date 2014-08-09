#ifndef GESTURES_DEVICE_H_
#define GESTURES_DEVICE_H_

#include "int_array.h"

int init_uinput(int_array keys);
int destroy_uinput(int fd);
void send_key(int fd, int key);

#endif // GESTURES_DEVICE_H_
