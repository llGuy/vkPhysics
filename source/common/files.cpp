#include "files.hpp"
#include "containers.hpp"

#include <stb_image.h>

//For now all that file_object_t holds
struct file_object_t {
    FILE *file;
    const char *path;
    uint32_t type;
};

static stack_container_t<file_object_t> files;

void files_init() {
    files.init(150);
}

const char *create_real_path(const char *path) {
    uint32_t strlen_path = (uint32_t)strlen(path);
    uint32_t strlen_root = (uint32_t)strlen(PROJECT_ROOT);
    
    char *final_path = LN_MALLOC(char, strlen_path + strlen_root + 2);
    memcpy(final_path, PROJECT_ROOT, strlen_root);
    // May need to vary for Windows
    final_path[strlen_root] = '/';
    memcpy(final_path + strlen_root + 1, path, strlen_path + 1);

    return final_path;
}

file_handle_t create_file(
    const char *file,
    uint32_t type) {
    file_handle_t handle = files.add();
    file_object_t *object = files.get(handle);

    object->path = create_real_path(file);
    object->type = type;

    if (type & FLF_IMAGE) {
        object->file = NULL;
    }
    else {
        char flags[3] = { 'r', '\0', '\0' };
        if (type & FLF_WRITEABLE) {
            flags[0] = 'w';
        }
        if (type & FLF_BINARY) {
            flags[1] = 'b';
        }
        if (type & FLF_OVERWRITE) {
            flags[1] = '+';
        }

        object->file = fopen(object->path, flags);
    }

    return handle;
}

bool does_file_exist(file_handle_t handle) {
    return files.get(handle)->file != NULL;
}

file_contents_t read_file(
    file_handle_t handle) {
    file_object_t *object = files.get(handle);
    if (object->type & FLF_IMAGE) {
        int32_t width, height, channels;
        uint8_t *image_data = (uint8_t *)stbi_load(object->path, &width, &height, &channels, STBI_rgb_alpha);

        file_contents_t contents = {};
        contents.pixels = image_data;
        contents.width = width;
        contents.height = height;
        contents.channels = channels;

        return contents;
    }
    else {
        fseek(object->file, 0L, SEEK_END);
        uint32_t file_size = ftell(object->file);
        uint8_t *data = LN_MALLOC(uint8_t, file_size);
        rewind(object->file);
        fread(data, sizeof(uint8_t), file_size, object->file);

        file_contents_t contents = {};
        contents.data = data;
        contents.size = file_size;

        return contents;
    }
}

void write_file(
    file_handle_t handle,
    uint8_t *bytes,
    uint32_t size) {
    file_object_t *object = files.get(handle);

    fwrite(bytes, 1, size, object->file);
}

void free_file(
    file_handle_t handle) {
    file_object_t *object = files.get(handle);

    if (object->file) {
        fclose(object->file);
    }

    files.remove(handle);
}

void free_image(
    file_contents_t contents) {
    stbi_image_free(contents.pixels);
}
