#include "fx_post.hpp"
#include <vk.hpp>

// This struct gets passed to rendering so that the renderer can choose the post processing
// Passes to use against the scene
static vk::frame_info_t frame_info;

void fx_enable_blur() {
    frame_info.blurred = 1;
}

void fx_enable_ssao() {
    frame_info.ssao = 1;
}

void fx_disable_blur() {
    frame_info.blurred = 0;
}

void fx_disable_ssao() {
    frame_info.ssao = 0;
}

vk::frame_info_t *fx_get_frame_info() {
    return &frame_info;
}
