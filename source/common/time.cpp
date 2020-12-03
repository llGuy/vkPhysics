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
    std::this_thread::sleep_for(std::chrono::duration<float>(seconds));

// #ifdef _WIN32
//     Sleep((DWORD)(seconds * 1000.0f));
// #else
//     timespec ts;
//     int32_t ret;
//     ts.tv_sec = 0;
//     ts.tv_nsec = (15) * 1000000;
//     do {
//         ret = nanosleep(&ts, &ts);
//     } while (ret && errno == EINTR);
// #endif    
}

// Get global time
