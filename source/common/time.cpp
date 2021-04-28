#include "time.hpp"
#include <thread>

time_stamp_t current_time() {
    return std::chrono::high_resolution_clock::now();
}

float time_difference(
    time_stamp_t end,
    time_stamp_t start) {
    std::chrono::duration<float> seconds = end - start;
    float delta = seconds.count();
    return (float)delta;
}

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <errno.h>
#endif

void sleep_seconds(
    float seconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds((uint32_t)(seconds * 1000.0f)));
}

// Get global time
