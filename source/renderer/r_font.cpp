#include "renderer.hpp"
#include <common/files.hpp>
#include <common/allocators.hpp>

// Loading fonts
struct fnt_word_t {
    char *pointer;
    uint16_t size;
};

// Except new line
static char *s_fnt_skip_break_characters(
    char *pointer) {
    for (;;) {
        switch (*pointer) {
        case ' ': case '=': ++pointer;
        default: return pointer;
        }
    }
    return NULL;
}

static char *s_fnt_goto_next_break_character(
    char *pointer) {
    for (;;) {
        switch (*pointer) {
        case ' ': case '=': case '\n': return pointer;
        default: ++pointer;
        }
    }
    return NULL;
}

static char *s_fnt_skip_line(
    char *pointer) {
    for (;;) {
        switch (*pointer) {
        case '\n': return ++pointer;
        default: ++pointer;
        }
    }
    return NULL;
}

static char *s_fnt_move_and_get_word(
    char *pointer,
    fnt_word_t *dst_word) {
    char *new_pointer = s_fnt_goto_next_break_character(pointer);
    if (dst_word) {
        dst_word->pointer = pointer;
        dst_word->size = (int16_t)(new_pointer - pointer);
    }
    return(new_pointer);
}

static char *s_fnt_skip_until_digit(
    char *pointer) {
    for (;;) {
        switch (*pointer) {
        case '-': case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9': return pointer;
        default: ++pointer;
        }
    }
    return NULL;
}

struct fnt_string_number_t {
    fnt_word_t word;
    // Maximum 10 chars for a number
    char characters[10];
};

static int32_t s_fnt_atoi(
    fnt_word_t word) {
    fnt_string_number_t number;
    memcpy(number.characters, word.pointer, sizeof(char) * word.size);
    number.characters[word.size] = '\0';
    return(atoi(number.characters));
}

static char *s_fnt_get_char_count(
    char *pointer,
    int32_t *count) {
    for (;;) {
        fnt_word_t chars_str;
        pointer = s_fnt_move_and_get_word(pointer, &chars_str);
        if (chars_str.size == strlen("chars")) {
            pointer = s_fnt_skip_until_digit(pointer);
            fnt_word_t count_str;
            pointer = s_fnt_move_and_get_word(pointer, &count_str);
            *count = s_fnt_atoi(count_str);
            pointer = s_fnt_skip_line(pointer);
            return pointer;
        }
        pointer = s_fnt_skip_line(pointer);
    }
}

static char *s_fnt_get_font_attribute_value(
    char *pointer,
    int32_t *value) {
    pointer = s_fnt_skip_until_digit(pointer);
    fnt_word_t value_str;
    pointer = s_fnt_move_and_get_word(pointer, &value_str);
    *value = s_fnt_atoi(value_str);
    return(pointer);
}

font_t *load_font(
    const char *fnt_file,
    const char *png_file) {
    // TODO : Make sure this is parameterised, not fixed!
    static constexpr float FNT_MAP_W = 512.0f, FNT_MAP_H = 512.0f;

    font_t *font = FL_MALLOC(font_t, 1);
    
    file_handle_t fnt_file_handle = create_file(
        fnt_file,
        FLF_TEXT);

    file_contents_t fnt = read_file(
        fnt_file_handle);

    int32_t char_count = 0;
    char *current_char = s_fnt_get_char_count((char *)fnt.data, &char_count);

    // Ready to start parsing the file
    for (uint32_t i = 0; i < (uint32_t)char_count; ++i) {
        // Char ID value
        int32_t char_id = 0;
        current_char = s_fnt_get_font_attribute_value(current_char, &char_id);

        // X value
        int32_t x = 0;
        current_char = s_fnt_get_font_attribute_value(current_char, &x);

        // X value
        int32_t y = 0;
        current_char = s_fnt_get_font_attribute_value(current_char, &y);
        y = (int32_t)FNT_MAP_H - y;
        
        // Width value
        int32_t width = 0;
        current_char = s_fnt_get_font_attribute_value(current_char, &width);

        // Height value
        int32_t height = 0;
        current_char = s_fnt_get_font_attribute_value(current_char, &height);

        // XOffset value
        int32_t xoffset = 0;
        current_char = s_fnt_get_font_attribute_value(current_char, &xoffset);

        // YOffset value
        int32_t yoffset = 0;
        current_char = s_fnt_get_font_attribute_value(current_char, &yoffset);

        // XAdvanc value
        int32_t xadvance = 0;
        current_char = s_fnt_get_font_attribute_value(current_char, &xadvance);

        font_character_t *character = &font->font_characters[char_id];
        character->character_value = (char)char_id;
        // ----------------------------------------------------------------------------- \/ Do y - height so that base position is at bottom of character
        character->uvs_base = vector2_t((float)x / (float)FNT_MAP_W, (float)(y - height) / (float)FNT_MAP_H);
        character->uvs_size = vector2_t((float)width / (float)FNT_MAP_W, (float)height / (float)FNT_MAP_H);
        character->display_size = vector2_t((float)width / (float)xadvance, (float)height / (float)xadvance);
        character->offset = vector2_t((float)xoffset / (float)xadvance, (float)yoffset / (float)xadvance);
        character->offset.y *= -1.0f;
        character->advance = (float)xadvance / (float)xadvance;
        
        current_char = s_fnt_skip_line(current_char);
    }

    free_file(
        fnt_file_handle);

    font->font_img = create_texture(
        png_file,
        VK_FORMAT_R8G8B8A8_UNORM,
        NULL,
        0, 0,
        VK_FILTER_LINEAR);

    return font;
}

void ui_text_t::init(
    ui_box_t *box,
    font_t *in_font,
    font_stream_box_relative_to_t in_relative_to,
    float px_x_start,
    float px_y_start,
    uint32_t in_chars_per_line,
    float in_line_height) {
    this->dst_box = box;
    this->font = in_font;
    this->relative_to = in_relative_to;
    this->x_start = px_x_start;
    this->y_start = px_y_start;
    this->chars_per_line = in_chars_per_line;
    this->line_height = in_line_height;
}

void ui_text_t::draw_char(
    char character,
    uint32_t color) {
    colors[char_count] = color;
    characters[char_count++] = character;
}

void ui_text_t::draw_string(
    const char *string,
    uint32_t color) {
    uint32_t string_length = (uint32_t)strlen(string);
    memcpy(characters + char_count, string, sizeof(char) * string_length);
    for (uint32_t i = 0; i < string_length; ++i) {
        colors[char_count + i] = color;
    }
    char_count += string_length;
}

void ui_text_t::null_terminate() {
    characters[char_count] = 0;
}

// void ui_input_text_t::input(raw_input_t *raw_input) {
//     for (uint32_t i = 0; i < raw_input->char_count; ++i) {
//         char character[2] = {raw_input->char_stack[i], 0};
//         if (character[0]) {
//             text.colors[cursor_position] = text_color;
//             text.characters[cursor_position++] = raw_input->char_stack[i];
//             ++text.char_count;
//             raw_input->char_stack[i] = 0;
//         }
//     }

//     if (raw_input->buttons[button_type_t::BACKSPACE].state) {
//         if (text.char_count && cursor_position) {
//             --cursor_position;
//             --text.char_count;
//         }
//     }
// }

