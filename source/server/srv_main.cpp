#include <sha1.hpp>
#include "srv_game.hpp"
#include "nw_server.hpp"
#include <common/time.hpp>
#include <common/meta.hpp>
#include <common/game.hpp>
#include <common/files.hpp>
#include <common/event.hpp>
#include <renderer/input.hpp>
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

        timestep_begin(dt);

        dispatch_events(&events);

        LN_CLEAR();

        nw_tick(&events);

#if 0
        char *output = check_request_finished();
        if (output) {
            LOG_INFO("Got back to main thread\n");
        }
#endif

        srv_game_tick();

        timestep_end();

        // Sleep to not kill CPU usage
        time_stamp_t current = current_time();
        float dt = time_difference(current, tick_start);
        if (dt < 1.0f / 100.0f) {
            sleep_seconds((1.0f / 100.0f) - dt);
        }

        s_end_time();
    }
}

static void s_parse_command_line_args(int32_t argc, char *argv[]) {
    if (argc >= 2) {
        event_start_server_t *data = FL_MALLOC(event_start_server_t, 1);
        uint32_t server_name_length = strlen(argv[1]);
        char *server_name = FL_MALLOC(char, server_name_length + 1);
        server_name[server_name_length] = 0;
        strcpy(server_name, argv[1]);
        for (uint32_t c = 0; c < server_name_length; ++c) {
            if (server_name[c] == '_') {
                server_name[c] = ' ';
            }
        }

        data->server_name = server_name;

        submit_event(ET_START_SERVER, data, &events);
    }
    else {
        LOG_ERROR("Incorrect command line arguments, correct usage is (underscores will be replaced by spaces): ./vkPhysics_server My_awesome_server_name\n");
    }
}

// Entry point for client program
int32_t main(
    int32_t argc,
    char *argv[]) {
    global_linear_allocator_init((uint32_t)megabytes(30));
    srand(time(NULL));
    running = 1;
    files_init();

    { // For testing hashing
        uint8_t hash[20];

        SHA1_CTX ctx;
        SHA1Init(&ctx);

        uint8_t str[] = "Hello";
        SHA1Update(&ctx, str, 5);
        SHA1Final(hash, &ctx);
    }

    nw_init(&events);

    game_memory_init();
    srv_game_init(&events);

    // s_parse_command_line_args(argc, argv);

#if 0
    { // Just for testing meta server stuff
        send_request(R_SIGN_UP);
    }
#endif
 
    s_run();

    dispatch_events(&events);
    dispatch_events(&events);

    return 0;
}

float srv_delta_time() {
    return dt;
}
