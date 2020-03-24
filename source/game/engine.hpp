#pragma once

#include <common/tools.hpp>

enum game_init_flags_t : int32_t {
                                  GIF_WINDOWED = 1 << 0,
                                  GIF_NOT_WINDOWED = 1 << 1,
                                  GIF_CLIENT = 1 << 2,
                                  GIF_SERVER = 1 << 3
};

struct game_init_data_t {
    uint32_t fl_pool_size;
    uint32_t ln_pool_size;
    int32_t flags;
};

void game_main(
    game_init_data_t *game_init_data);

float logic_delta_time();

#define INFO_LOG(str) \
    printf("INFO: %s", str); \
    putchar('\n')

#define ERROR_LOG(str) \
    printf("ERROR: %s", str); \
    putchar('\n')

#define FINFO_LOG(str, ...) \
    printf("INFO: "); \
    printf(str, __VA_ARGS__) \
    putchar('\n')

#define FERROR_LOG(str, ...) \
    printf("ERROR: "); \
    printf(str, __VA_ARGS__) \
    putchar('\n')
