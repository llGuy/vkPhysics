#pragma once

#include <stdio.h>

/*
  #define NET_DEBUG_VOXEL_INTERPOLATION 1
  #define NET_DEBUG 1
  #define NET_DEBUG_TERRAFORMING 1
*/

namespace net {

/*
  In the (hopefully as rare as possible) case in which we need to debug
  nasty networking bugs.
*/

#if NET_DEBUG_TERRAFORMING
template <typename ...T> void debug_log(
    const char *format,
    bool print_to_console,
    FILE *log_file,
    T ...values) {
    fprintf(log_file, format, values...);

    if (print_to_console) {
        printf(format, values...);
    }
}
#else
template<typename ...T> void debug_log(
    const char *format,
    bool print_to_console,
    FILE *log_file,
    T ...values) {
    // Don't do anything
}
#endif

}
