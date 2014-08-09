#ifndef INTEGER_ARRAY_H_
#define INTEGER_ARRAY_H_

#include <stddef.h>

typedef struct t_int_array {
  size_t length;
  int data[1];
} int_array;

int_array * new_int_array(size_t length);

#endif // INTEGER_ARRAY_H_
