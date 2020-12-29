#include <sha1.hpp>
#include <signal.h>
#include "nw_server_meta.hpp"
#include "srv_game.hpp"
#include "nw_server.hpp"
#include <time.hpp>
#include <files.hpp>
#include <allocators.hpp>

#include <vkph_state.hpp>
#include <vkph_events.hpp>

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

static void s_run() {
    while (running) {
        s_begin_time();

        state->timestep_begin(dt);

        vkph::dispatch_events();

        LN_CLEAR();

        srv_game_tick(state);
        nw_tick(state);

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
    nw_deactivate_server();

    LOG_INFO("Stopped running server\n");

    exit(signum);
}

// Entry point for client program
int32_t main(
    int32_t argc,
    char *argv[]) {
    signal(SIGINT, s_handle_interrupt);

    global_linear_allocator_init((uint32_t)megabytes(30));
    srand(time(NULL));
    running = 1;
    files_init();

    state = flmalloc<vkph::state_t>();

    nw_init(state);
    srv_game_init(state);

    s_run();

    nw_deactivate_server();

    vkph::dispatch_events();
    vkph::dispatch_events();

    return 0;
}

float srv_delta_time() {
    return dt;
}
