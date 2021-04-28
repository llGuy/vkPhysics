#include "app_context.hpp"
#include <constant.hpp>

#include <stdio.h>
#include <stdlib.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <log.hpp>
#include <files.hpp>
#include <time.hpp>
#include <vkph_events.hpp>
#include <vkph_event_data.hpp>

namespace app {

float g_delta_time = 0.0f;

static struct {
    GLFWwindow *window;
    const char *name;
    raw_input_t raw_input;
} ctx;

void api_init() {
    if (!glfwInit()) {
        LOG_ERROR("Failed to initialize GLFW\n");
        exit(1);
    }
}

static void s_create_vulkan_surface_proc(
    struct VkInstance_T *instance,
    struct VkSurfaceKHR_T **surface,
    void *window) {
    if (glfwCreateWindowSurface(instance, (GLFWwindow *)window, NULL, surface) != VK_SUCCESS) {
        LOG_ERROR("Failed to create surface\n");
        exit(1);
    }
}

static void s_window_resize_callback(
    GLFWwindow *window,
    int32_t width,
    int32_t height) {
    (void)window;
    ctx.raw_input.resized = 1;
    ctx.raw_input.window_width = width;
    ctx.raw_input.window_height = height;
}

enum key_action_t { KA_DOWN, KA_RELEASE };

static void s_set_button_state(
    button_type_t button,
    button_state_t state) {
    ctx.raw_input.buttons[button].state = state;
    if (state == BS_DOWN) {
        ctx.raw_input.buttons[button].down_amount += g_delta_time;
        ctx.raw_input.buttons[button].instant = 1;
        ctx.raw_input.buttons[button].release = 0;
        ctx.raw_input.instant_indices[ctx.raw_input.instant_count++] = button;
    }
    else if (state == BS_NOT_DOWN) {
        ctx.raw_input.buttons[button].down_amount = 0.0f;
        ctx.raw_input.buttons[button].instant = 0;
        ctx.raw_input.buttons[button].release = 1;
        ctx.raw_input.release_indices[ctx.raw_input.release_count++] = button;
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
    } break;

    case GLFW_RELEASE: {
        state = BS_NOT_DOWN;
    } break;
            
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
    case GLFW_KEY_F9: { s_set_button_state(BT_F9, state); } break;
    }
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
    } break;

    case GLFW_RELEASE: {
        state = BS_NOT_DOWN;
    } break;
            
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
    if (ctx.raw_input.char_count != MAX_CHARS) {
        if (codepoint >= 32) {
            ctx.raw_input.char_stack[ctx.raw_input.char_count++] = (char)codepoint;
        }
    }
}

static void s_window_cursor_position_callback(
    GLFWwindow* window,
    double xpos,
    double ypos) {
    ctx.raw_input.cursor_moved = 1;

    ctx.raw_input.cursor_pos_x = (float)xpos;
    ctx.raw_input.cursor_pos_y = (float)ypos;
}

context_info_t init_context(const char *name) {
    ctx.name = name;
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    const GLFWvidmode *vidmode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    int32_t width = vidmode->width / 2;
    int32_t height = vidmode->height / 2;

    ctx.window = glfwCreateWindow(width, height, ctx.name, NULL, NULL);

    glfwSetWindowSizeCallback(ctx.window, s_window_resize_callback);
    glfwSetKeyCallback(ctx.window, s_window_key_callback);
    glfwSetMouseButtonCallback(ctx.window, s_window_mouse_button_callback);
    glfwSetCharCallback(ctx.window, s_window_character_callback);
    glfwSetCursorPosCallback(ctx.window, s_window_cursor_position_callback);

    { // Set icon
        file_handle_t img = create_file("assets/screenshots/epic_logo.png", FLF_IMAGE);
        file_contents_t contents = read_file(img);

        GLFWimage image = {};
        image.pixels = (unsigned char *)contents.pixels;
        image.width = contents.width;
        image.height = contents.height;

        glfwSetWindowIcon(ctx.window, 1, &image);

        free_image(contents);
        free_file(img);
    }

    glfwGetFramebufferSize(ctx.window, &width, &height);

    context_info_t ctx_info = {};
    ctx_info.surface_proc = &s_create_vulkan_surface_proc;
    ctx_info.width = width;
    ctx_info.height = height;
    ctx_info.name = ctx.name;
    ctx_info.window = (void *)ctx.window;

    return ctx_info;
}

const raw_input_t *get_raw_input() {
    return &ctx.raw_input;
}

void poll_input_events() {
    static bool initialised_time = 0;
    static float current_time = 0.0f;

    if (!initialised_time) {
        current_time = (float)glfwGetTime();
        initialised_time = 1;
    }
    
    // Reset instant presses
    for (uint32_t i = 0; i < ctx.raw_input.instant_count; ++i) {
        uint32_t index = ctx.raw_input.instant_indices[i];
        ctx.raw_input.buttons[index].instant = 0;
    }

    for (uint32_t i = 0; i < ctx.raw_input.release_count; ++i) {
        uint32_t index = ctx.raw_input.release_indices[i];
        ctx.raw_input.buttons[index].release = 0;
    }

    ctx.raw_input.instant_count = 0;
    ctx.raw_input.release_count = 0;
    ctx.raw_input.char_count = 0;

    ctx.raw_input.previous_cursor_pos_x = ctx.raw_input.cursor_pos_x;
    ctx.raw_input.previous_cursor_pos_y = ctx.raw_input.cursor_pos_y;
    ctx.raw_input.cursor_moved = 0;

    glfwPollEvents();

    g_delta_time = (float)glfwGetTime() - current_time;

    static constexpr float FRAME_TIME = 1.0f / 100.0f;

    // When debugging, we don't want dt to be ginormous.
#if defined(DEBUGGING)
    if (g_delta_time > 1.0f) {
        g_delta_time = 1.0f / 100.0f;
    }
#endif

#if 0
    if (g_delta_time < FRAME_TIME) {
        float wait_time = FRAME_TIME - g_delta_time;

        sleep_seconds(wait_time);

        g_delta_time = FRAME_TIME;
    }
#endif

    // Start timing
    current_time = (float)glfwGetTime();

    if (glfwWindowShouldClose(ctx.window)) {
        vkph::submit_event(vkph::ET_CLOSED_WINDOW, NULL);
    }

    if (ctx.raw_input.buttons[BT_ESCAPE].instant) {
        vkph::submit_event(vkph::ET_PRESSED_ESCAPE, NULL);
    }
    
    // Check if user resized to trigger event
    if (ctx.raw_input.buttons[BT_F11].instant) {
        if (ctx.raw_input.in_fullscreen) {
            glfwSetWindowMonitor(
                ctx.window,
                NULL,
                ctx.raw_input.window_pos_x,
                ctx.raw_input.window_pos_y,
                ctx.raw_input.desktop_window_width,
                ctx.raw_input.desktop_window_height,
                0);

            glfwGetFramebufferSize(ctx.window, &ctx.raw_input.window_width, &ctx.raw_input.window_height);
            
            ctx.raw_input.in_fullscreen = 0;
            ctx.raw_input.resized = 1;
        }
        else {
            glfwGetWindowPos(ctx.window, &ctx.raw_input.window_pos_x, &ctx.raw_input.window_pos_y);
            glfwGetWindowSize(ctx.window, &ctx.raw_input.desktop_window_width, &ctx.raw_input.desktop_window_height);
            //ctx.raw_input.desktop_window_width = ctx.raw_input.window_width;
            //ctx.raw_input.desktop_window_height = ctx.raw_input.window_height;
            
            const GLFWvidmode *vidmode = glfwGetVideoMode(glfwGetPrimaryMonitor());

            glfwSetWindowMonitor(ctx.window, glfwGetWindowMonitor(ctx.window), 0, 0, vidmode->width, vidmode->height, 0);

            ctx.raw_input.resized = 1;
            ctx.raw_input.in_fullscreen = 1;
            ctx.raw_input.window_width = vidmode->width;
            ctx.raw_input.window_height = vidmode->height;
        }
    }
    
    if (ctx.raw_input.buttons[BT_F9].instant) {
        printf("Break code\n");
    }

    if (ctx.raw_input.resized) {
        // TODO: Change this to linear allocator when allocators are added in the future
        vkph::event_surface_resize_t *resize_data = flmalloc<vkph::event_surface_resize_t>(1);
        resize_data->width = ctx.raw_input.window_width;
        resize_data->height = ctx.raw_input.window_height;
        
        submit_event(vkph::ET_RESIZE_SURFACE, resize_data);

        ctx.raw_input.resized = 0;
    }
}

void enable_cursor() {
    glfwSetInputMode(ctx.window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}

void disable_cursor() {
    glfwSetInputMode(ctx.window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

void get_immediate_cursor_pos(double *x, double *y) {
    glfwGetCursorPos(ctx.window, x, y);
}

void nullify_char_at(uint32_t i) {
    ctx.raw_input.char_stack[i] = 0;
}

}
