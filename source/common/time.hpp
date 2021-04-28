#pragma once

#include <chrono>

using time_stamp_t = std::chrono::high_resolution_clock::time_point;

time_stamp_t current_time();

float time_difference(
    time_stamp_t end,
    time_stamp_t start);

void sleep_seconds(
    float seconds);
