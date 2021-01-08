#include <sha1.hpp>
#include <signal.h>
#include "srv_net.hpp"
#include "srv_net_meta.hpp"
#include "srv_game.hpp"
#include <time.hpp>
#include <files.hpp>
#include <allocators.hpp>

#include <vkph_state.hpp>
#include <vkph_events.hpp>

namespace srv {

static bool running;

/*
  All the game state is in here.
*/
static vkph::state_t *state;

static float dt;

static time_stamp_t tick_start;
static time_stamp_t tick_end;

static void s_begin_time() {
    tick_start = current_time();
}

static void s_end_time() {
    tick_end = current_time();

    dt = time_difference(tick_end, tick_start);
    state->delta_time = dt;
}

static void s_loop() {
    while (running) {
        s_begin_time();

        state->timestep_begin(dt);

        vkph::dispatch_events();

        LN_CLEAR();

        tick_game(state);
        tick_net(state);

        state->timestep_end();

        // Sleep to not kill CPU usage
        time_stamp_t current = current_time();
        float dt = time_difference(current, tick_start);
        if (dt < 1.0f / 100.0f) {
            sleep_seconds((1.0f / 100.0f) - dt);
        }

        s_end_time();
    }
}

static void s_handle_interrupt(int signum) {
    deactivate_server();

    LOG_INFO("Stopped running server\n");

    exit(signum);
}

int32_t run(
    int32_t argc,
    char *argv[]) {
    signal(SIGINT, s_handle_interrupt);

    global_linear_allocator_init((uint32_t)megabytes(30));
    srand((uint32_t)time(NULL));
    running = 1;
    files_init();

    state = flmalloc<vkph::state_t>();

    init_net(state);
    init_game(state);

    LN_CLEAR();

    s_loop();

    deactivate_server();

    vkph::dispatch_events();
    vkph::dispatch_events();

    return 0;
}

float delta_time() {
    return dt;
}

}

// Entry point for server program
int32_t main(
    int32_t argc,
    char *argv[]) {
    return srv::run(argc, argv);
}
