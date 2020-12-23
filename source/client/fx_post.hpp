#pragma once

#include <vk.hpp>

void fx_enable_blur();
void fx_enable_ssao();

void fx_disable_blur();
void fx_disable_ssao();

vk::frame_info_t *fx_get_frame_info();
