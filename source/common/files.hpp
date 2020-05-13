#pragma once

typedef int32_t file_handle_t;

enum {
    INVALID_FILE_HANDLE = -1
};

enum file_load_flags_t {
    TEXT,
    BINARY = 1 << 1,
    IMAGE = 1 << 2,
    ASSET = 1 << 3
};
