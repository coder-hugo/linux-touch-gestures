#ifndef COMMON_H_
#define COMMON_H_

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#define die(str, args...) do { \
        perror(str); \
        exit(EXIT_FAILURE); \
    } while(0)

#endif // COMMON_H_
