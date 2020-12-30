#pragma once

#include <stdint.h>

#include "vkph_team.hpp"
#include "vkph_event.hpp"
#include "vkph_player.hpp"

namespace vkph {

struct event_surface_resize_t {
    uint32_t width;
    uint32_t height;
};

struct event_start_client_t {
    const char *client_name;
};

struct event_start_server_t {
    // Clients will see these names with looking at servers list
    const char *server_name;
};

struct event_data_request_to_join_server_t {
    // Program can either provide server name or raw ip address: both can work
    // Value copy of game server information
    const char *server_name;
    const char *ip_address;
};

struct event_enter_server_t {
    uint16_t local_client_id;

    uint32_t info_count;
    player_init_info_t *infos;

    // Will need to have other stuff
};

struct event_spawn_t {
    /*
      If the client_id is -1, the local player is requesting to spawn.
     */
    int32_t client_id;
};

struct event_new_player_t {
    player_init_info_t info;
};

struct event_player_disconnected_t {
    uint16_t client_id;
};

struct event_chunk_voxel_packet_t {
    struct packet_chunk_voxels_t *packet;
};

struct event_set_chunk_history_tracker_t {
    bool value;
};

struct fade_trigger_t {
    event_type_t trigger_type;
    void *next_event_data;
};

struct event_begin_fade_effect_t {
    float dest_value;
    float duration;

    // Can make the fade effect trigger another event (like opening a menu or something)
    uint32_t trigger_count = 0;
    fade_trigger_t triggers[5];
    
    bool fade_back;
};

enum ai_training_session_t {
    ATS_WALKING,
    ATS_ROLLING,
};

struct event_begin_ai_training_t {
    ai_training_session_t session_type;
};

struct event_attempt_sign_up_t {
    const char *username;
    const char *password;
};

struct event_attempt_login_t {
    const char *username;
    const char *password;
};

struct event_meta_request_error_t {
    uint32_t error_type;
};

struct event_enter_map_creator_t {
    const char *map_path;
    const char *map_name;
};

struct event_map_editor_chose_color_t {
    uint8_t r, g, b;
};

struct event_send_server_team_select_request_t {
    team_color_t color;
};

}
