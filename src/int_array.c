#include <stdlib.h>

#include "int_array.h"

int_array *new_int_array(size_t length) {
  int_array *array;
  /* we're allocating the size of basic t_int_array 
     (which already contains space for one int)
     and additional space for length-1 ints */
  array = malloc(sizeof(int_array) + sizeof(int) * (length - 1));
  if(!array) {
    return 0;
  }
  array->length = length;
  return array;
}
