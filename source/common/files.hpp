#pragma once

#include "tools.hpp"

typedef int32_t file_handle_t;

// Every file that is loaded will have its root begin at the project directory
// Example: ~/path/to/vkPhysics/
// So if you load a file with path shaders/lighting.frag, it will load
// ~/path/to/vkPhysics/shaders/lightign.frag

enum {
    INVALID_FILE_HANDLE = -1
};

enum file_load_flags_t {
    FLF_TEXT,
    FLF_BINARY = 1 << 1,
    FLF_IMAGE = 1 << 2,
    FLF_WRITEABLE = 1 << 3,
    FLF_OVERWRITE = 1 << 4,
    FLF_NONE = 1 << 5
};

void files_init();

// Creates the real path of the file.
// Path to pass in must have root as project root.
file_handle_t create_file(
    const char *file,
    uint32_t type);
    
const char *create_real_path(const char *path);

void delete_file(
    const char *file);

bool does_file_exist(file_handle_t handle);

struct file_contents_t {
    bool fl_allocated;
    union {
        // If this is not an image
        struct {
            uint32_t size;
            uint8_t *data;
        };

        // If this is an image
        struct {
            int32_t width, height;
            void *pixels;
            int32_t channels;
        };
    };
};

file_contents_t read_file(file_handle_t handle);
void free_file_contents(file_handle_t file, file_contents_t content);
void write_file(file_handle_t file, uint8_t *bytes, uint32_t size);
void free_file(file_handle_t handle);
void free_image(file_contents_t contents);
