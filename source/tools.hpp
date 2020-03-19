#pragma once

#include "t_types.hpp"

#define FL_MALLOC(type, n) (type *)malloc(sizeof(type) * (n))
#define FL_FREE(ptr) free(ptr)

#define ALLOCA(type, n) (type *)alloca(sizeof(type) * (n))

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
