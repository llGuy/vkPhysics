#include <sha1.hpp>
#include <signal.h>
#include "nw_server_meta.hpp"
#include "srv_game.hpp"
#include "nw_server.hpp"
#include <common/time.hpp>
#include <common/meta.hpp>
#include <common/game.hpp>
#include <common/files.hpp>
#include <common/event.hpp>
#include <common/allocators.hpp>

static bool running;

// All event submissions go here
static event_submissions_t events;

static float dt;

static time_stamp_t tick_start;
static time_stamp_t tick_end;

static void s_begin_time() {
    tick_start = current_time();
}

static void s_end_time() {
    tick_end = current_time();

    dt = time_difference(tick_end, tick_start);
}

static void s_run() {
    while (running) {
        s_begin_time();

        g_game->timestep_begin(dt);

        dispatch_events(&events);

        LN_CLEAR();

        srv_game_tick();
        nw_tick(&events);

        g_game->timestep_end();

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

    nw_init(&events);

    game_allocate();
    srv_game_init(&events);

    s_run();

    nw_deactivate_server();

    dispatch_events(&events);
    dispatch_events(&events);

    return 0;
}

float srv_delta_time() {
    return dt;
}
