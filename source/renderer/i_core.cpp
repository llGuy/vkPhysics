// Input core

#include <stdio.h>
#include "input.hpp"
#include <common/tools.hpp>
#include <common/allocators.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

static float sdelta_time = 0.0f;

float surface_delta_time() {
    return sdelta_time;
}

static void s_create_vulkan_surface_proc(
    struct VkInstance_T *instance,
    struct VkSurfaceKHR_T **surface,
    void *window) {
    if (glfwCreateWindowSurface(instance, (GLFWwindow *)window, NULL, surface) != VK_SUCCESS) {
        printf("Failed to create surface\n");
        exit(1);
    }
}

static raw_input_t raw_input = {};

raw_input_t *get_raw_input() {
    return &raw_input;
}

static void s_window_resize_callback(
    GLFWwindow *window,
    int32_t width,
    int32_t height) {
    (void)window;
    raw_input.resized = 1;
    raw_input.window_width = width;
    raw_input.window_height = height;
}

enum key_action_t { KA_DOWN, KA_RELEASE };

static void s_set_button_state(
    button_type_t button,
    button_state_t state) {
    raw_input.buttons[button].state = state;
    if (state == BS_DOWN) {
        raw_input.buttons[button].down_amount += surface_delta_time();
        raw_input.buttons[button].instant = 1;
        raw_input.instant_indices[raw_input.instant_count++] = button;
    }
    else if (state == BS_NOT_DOWN) {
        raw_input.buttons[button].down_amount = 0.0f;
        raw_input.buttons[button].instant = 0;
    }
}

static void s_window_key_callback(
    GLFWwindow *window,
    int key,
    int scancode,
    int action,
    int mods) {
    (void)window;
    (void)scancode;
    (void)mods;

    button_state_t state = (button_state_t)0;
    switch(action) {
        
    case GLFW_PRESS: case GLFW_REPEAT: {
        state = BS_DOWN;
        break;
    }

    case GLFW_RELEASE: {
        state = BS_NOT_DOWN;
        break;
    }
            
    }
    
    switch(key) {
    case GLFW_KEY_A: { s_set_button_state(BT_A, state); } break;
    case GLFW_KEY_B: { s_set_button_state(BT_B, state); } break;
    case GLFW_KEY_C: { s_set_button_state(BT_C, state); } break;
    case GLFW_KEY_D: { s_set_button_state(BT_D, state); } break;
    case GLFW_KEY_E: { s_set_button_state(BT_E, state); } break;
    case GLFW_KEY_F: { s_set_button_state(BT_F, state); } break;
    case GLFW_KEY_G: { s_set_button_state(BT_G, state); } break;
    case GLFW_KEY_H: { s_set_button_state(BT_H, state); } break;
    case GLFW_KEY_I: { s_set_button_state(BT_I, state); } break;
    case GLFW_KEY_J: { s_set_button_state(BT_J, state); } break;
    case GLFW_KEY_K: { s_set_button_state(BT_K, state); } break;
    case GLFW_KEY_L: { s_set_button_state(BT_L, state); } break;
    case GLFW_KEY_M: { s_set_button_state(BT_M, state); } break;
    case GLFW_KEY_N: { s_set_button_state(BT_N, state); } break;
    case GLFW_KEY_O: { s_set_button_state(BT_O, state); } break;
    case GLFW_KEY_P: { s_set_button_state(BT_P, state); } break;
    case GLFW_KEY_Q: { s_set_button_state(BT_Q, state); } break;
    case GLFW_KEY_R: { s_set_button_state(BT_R, state); } break;
    case GLFW_KEY_S: { s_set_button_state(BT_S, state); } break;
    case GLFW_KEY_T: { s_set_button_state(BT_T, state); } break;
    case GLFW_KEY_U: { s_set_button_state(BT_U, state); } break;
    case GLFW_KEY_V: { s_set_button_state(BT_V, state); } break;
    case GLFW_KEY_W: { s_set_button_state(BT_W, state); } break;
    case GLFW_KEY_X: { s_set_button_state(BT_X, state); } break;
    case GLFW_KEY_Y: { s_set_button_state(BT_Y, state); } break;
    case GLFW_KEY_Z: { s_set_button_state(BT_Z, state); } break;
    case GLFW_KEY_0: { s_set_button_state(BT_ZERO, state); } break;
    case GLFW_KEY_1: { s_set_button_state(BT_ONE, state); } break;
    case GLFW_KEY_2: { s_set_button_state(BT_TWO, state); } break;
    case GLFW_KEY_3: { s_set_button_state(BT_THREE, state); } break;
    case GLFW_KEY_4: { s_set_button_state(BT_FOUR, state); } break;
    case GLFW_KEY_5: { s_set_button_state(BT_FIVE, state); } break;
    case GLFW_KEY_6: { s_set_button_state(BT_SIX, state); } break;
    case GLFW_KEY_7: { s_set_button_state(BT_SEVEN, state); } break;
    case GLFW_KEY_8: { s_set_button_state(BT_EIGHT, state); } break;
    case GLFW_KEY_9: { s_set_button_state(BT_NINE, state); } break;
    case GLFW_KEY_UP: { s_set_button_state(BT_UP, state); } break;
    case GLFW_KEY_LEFT: { s_set_button_state(BT_LEFT, state); } break;
    case GLFW_KEY_DOWN: { s_set_button_state(BT_DOWN, state); } break;
    case GLFW_KEY_RIGHT: { s_set_button_state(BT_RIGHT, state); } break;
    case GLFW_KEY_SPACE: { s_set_button_state(BT_SPACE, state); } break;
    case GLFW_KEY_LEFT_SHIFT: { s_set_button_state(BT_LEFT_SHIFT, state); } break;
    case GLFW_KEY_LEFT_CONTROL: { s_set_button_state(BT_LEFT_CONTROL, state); } break;
    case GLFW_KEY_ENTER: { s_set_button_state(BT_ENTER, state); } break;
    case GLFW_KEY_BACKSPACE: { s_set_button_state(BT_BACKSPACE, state); } break;
    case GLFW_KEY_ESCAPE: { s_set_button_state(BT_ESCAPE, state); } break;
    case GLFW_KEY_F11: { s_set_button_state(BT_F11, state); } break;
    }

    /*if (key == GLFW_KEY_F11 && action == GLFW_PRESS) {
        if (is_fullscreen) {

        }
        else {
            const GLFWvidmode *vidmode = glfwGetVideoMode(glfwGetPrimaryMonitor());

            glfwSetWindowMonitor(window, glfwGetWindowMonitor(window), 0, 0, vidmode->width, vidmode->height, 0);
        }
    }*/
}

static void s_window_mouse_button_callback(
    GLFWwindow *window,
    int32_t button,
    int32_t action,
    int32_t mods) {
    button_state_t state = (button_state_t)0;
    switch(action) {
        
    case GLFW_PRESS: case GLFW_REPEAT: {
        state = BS_DOWN;
        break;
    }

    case GLFW_RELEASE: {
        state = BS_NOT_DOWN;
        break;
    }
            
    }

    switch(button) {
    case GLFW_MOUSE_BUTTON_LEFT: { s_set_button_state(BT_MOUSE_LEFT, state); } break;
    case GLFW_MOUSE_BUTTON_RIGHT: { s_set_button_state(BT_MOUSE_RIGHT, state); } break;
    case GLFW_MOUSE_BUTTON_MIDDLE: { s_set_button_state(BT_MOUSE_MIDDLE, state); } break;
    }
}

static void s_window_character_callback(
    GLFWwindow* window,
    unsigned int codepoint) {
    if (raw_input.char_count != MAX_CHARS) {
        if (codepoint >= 32) {
            raw_input.char_stack[raw_input.char_count++] = (char)codepoint;
        }
    }
}

static void s_window_cursor_position_callback(
    GLFWwindow* window,
    double xpos,
    double ypos) {
    raw_input.cursor_moved = 1;

    raw_input.cursor_pos_x = (float)xpos;
    raw_input.cursor_pos_y = (float)ypos;
}

static GLFWwindow *window;

void *surface_window() {
    return window;
}

static void s_error_callback(int, const char *msg) {
    //printf(msg);
}

input_interface_data_t input_interface_init() {
    if (!glfwInit()) {
        printf("Failed to initialize GLFW\n");
        exit(1);
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    const char *application_name = "vkPhysics";

    const GLFWvidmode *vidmode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    int32_t width = vidmode->width / 2;
    int32_t height = vidmode->height / 2;

    window = glfwCreateWindow(width, height, application_name, NULL, NULL);

    glfwSetWindowSizeCallback(window, s_window_resize_callback);
    glfwSetKeyCallback(window, s_window_key_callback);
    glfwSetMouseButtonCallback(window, s_window_mouse_button_callback);
    glfwSetCharCallback(window, s_window_character_callback);
    glfwSetCursorPosCallback(window, s_window_cursor_position_callback);
    glfwSetErrorCallback(s_error_callback);

    glfwGetFramebufferSize(window, &width, &height);

    input_interface_data_t interface_data = {};
    interface_data.surface_creation_proc = &s_create_vulkan_surface_proc;
    interface_data.surface_width = width;
    interface_data.surface_width = height;
    interface_data.application_name = application_name;
    interface_data.window = (void *)window;

    return interface_data;
}

void poll_input_events(event_submissions_t *submissions) {
    static float current_time = 0.0f;
    
    // Reset instant presses
    for (uint32_t i = 0; i < raw_input.instant_count; ++i) {
        uint32_t index = raw_input.instant_indices[i];
        raw_input.buttons[index].instant = 0;
    }

    raw_input.instant_count = 0;

    raw_input.previous_cursor_pos_x = raw_input.cursor_pos_x;
    raw_input.previous_cursor_pos_y = raw_input.cursor_pos_y;
    raw_input.cursor_moved = 0;

    glfwPollEvents();

    sdelta_time = get_current_time() - current_time;
    // Start timing
    current_time = get_current_time();

    if (glfwWindowShouldClose(window)) {
        submit_event(ET_CLOSED_WINDOW, NULL, submissions);
    }

    if (raw_input.buttons[BT_ESCAPE].instant) {
        submit_event(ET_PRESSED_ESCAPE, NULL, submissions);
    }
    
    // Check if user resized to trigger event
    if (raw_input.buttons[BT_F11].instant) {
        if (raw_input.in_fullscreen) {
            glfwSetWindowMonitor(
                window,
                NULL,
                raw_input.window_pos_x,
                raw_input.window_pos_y,
                raw_input.desktop_window_width,
                raw_input.desktop_window_height,
                0);

            glfwGetFramebufferSize(window, &raw_input.window_width, &raw_input.window_height);
            
            raw_input.in_fullscreen = 0;
            raw_input.resized = 1;
        }
        else {
            glfwGetWindowPos(window, &raw_input.window_pos_x, &raw_input.window_pos_y);
            glfwGetWindowSize(window, &raw_input.desktop_window_width, &raw_input.desktop_window_height);
            //raw_input.desktop_window_width = raw_input.window_width;
            //raw_input.desktop_window_height = raw_input.window_height;
            
            const GLFWvidmode *vidmode = glfwGetVideoMode(glfwGetPrimaryMonitor());

            glfwSetWindowMonitor(window, glfwGetWindowMonitor(window), 0, 0, vidmode->width, vidmode->height, 0);

            raw_input.resized = 1;
            raw_input.in_fullscreen = 1;
            raw_input.window_width = vidmode->width;
            raw_input.window_height = vidmode->height;
        }
    }

    if (raw_input.resized) {
        // TODO: Change this to linear allocator when allocators are added in the future
        event_surface_resize_t *resize_data = FL_MALLOC(event_surface_resize_t, 1);
        resize_data->width = raw_input.window_width;
        resize_data->height = raw_input.window_height;
        
        submit_event(ET_RESIZE_SURFACE, resize_data, submissions);

        raw_input.resized = 0;
    }
}

void disable_cursor_display() {
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

void enable_cursor_display() {
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}

float get_current_time() {
    return (float)glfwGetTime();
}

// Settings
struct action_bound_mouse_keyboard_button_t {
    // Keyboard/Mouse button
    button_type_t bound_button;
};

static action_bound_mouse_keyboard_button_t bound_key_mouse_buttons[GIAT_INVALID_ACTION];

void game_input_settings_init() {
    bound_key_mouse_buttons[GIAT_OK].bound_button = BT_ENTER;
    bound_key_mouse_buttons[GIAT_CANCEL].bound_button = BT_ESCAPE;
    bound_key_mouse_buttons[GIAT_MENU].bound_button = BT_ESCAPE;

    bound_key_mouse_buttons[GIAT_MOVE_FORWARD].bound_button = BT_W;
    bound_key_mouse_buttons[GIAT_MOVE_LEFT].bound_button = BT_A;
    bound_key_mouse_buttons[GIAT_MOVE_BACK].bound_button = BT_S;
    bound_key_mouse_buttons[GIAT_MOVE_RIGHT].bound_button = BT_D;

    bound_key_mouse_buttons[GIAT_TRIGGER1].bound_button = BT_MOUSE_LEFT;
    bound_key_mouse_buttons[GIAT_TRIGGER2].bound_button = BT_MOUSE_RIGHT;
    bound_key_mouse_buttons[GIAT_TRIGGER3].bound_button = BT_R;
    bound_key_mouse_buttons[GIAT_TRIGGER4].bound_button = BT_SPACE;
    bound_key_mouse_buttons[GIAT_TRIGGER5].bound_button = BT_E;
    bound_key_mouse_buttons[GIAT_TRIGGER6].bound_button = BT_LEFT_SHIFT;
}

static game_input_t game_input;

game_input_t *get_game_input() {
    return &game_input;
}

static void s_set_button_action_state(
    uint32_t action) {
    button_input_t *raw_key_mouse_input = &raw_input.buttons[bound_key_mouse_buttons[action].bound_button];

    game_input.actions[action].state = raw_key_mouse_input->state;
    game_input.actions[action].down_amount = raw_key_mouse_input->down_amount;
    game_input.actions[action].instant = raw_key_mouse_input->instant;
}


void translate_raw_to_game_input() {
    for (uint32_t action = 0; action < GIAT_INVALID_ACTION; ++action) {
        s_set_button_action_state(action);
    }

    game_input.previous_mouse_x = raw_input.previous_cursor_pos_x;
    game_input.previous_mouse_y = raw_input.previous_cursor_pos_y;
    game_input.mouse_x = raw_input.cursor_pos_x;
    game_input.mouse_y = raw_input.cursor_pos_y;
}
