#pragma once

#include <stdint.h>

namespace vkph {

enum class game_mode_t { DEATHMATCH, CAPTURE_THE_FLAG, INVALID };

// Stuff is public by design so that the client and server code can easily manipulate
// The game state (whether it's for client-side prediction, or interpolating snapshots)
// The methods are here to make life a little easier
struct game_t {

    // Core ///////////////////////////////////////////////////////////////////
    float dt;
    uint64_t current_tick;
    game_mode_t game_mode;

    // Teams //////////////////////////////////////////////////////////////////
    uint32_t team_count;
    team_t *teams;
    uint32_t team_color_to_index[(uint32_t)team_color_t::COUNT];

    // Players ////////////////////////////////////////////////////////////////
    stack_container_t<player_t *> players;
    int16_t client_to_local_id_map[PLAYER_MAX_COUNT];

    // Map ////////////////////////////////////////////////////////////////////
    const char *current_map_path;
    map_t *current_map_data;

    // Chunks /////////////////////////////////////////////////////////////////
    stack_container_t<chunk_t *> chunks;
    hash_table_t<uint32_t, 1500, 30, 10> chunk_indices;
    uint32_t max_modified_chunks;
    uint32_t modified_chunk_count;
    chunk_t **modified_chunks;

    struct {
        uint8_t track_history: 1;
    } flags;

    // Projectiles ////////////////////////////////////////////////////////////
    projectile_tracker_t<rock_t, PROJECTILE_MAX_ROCK_COUNT> rocks;
    stack_container_t<predicted_projectile_hit_t> predicted_hits;



    // Methods ////////////////////////////////////////////////////////////////
    void init_memory();

    // Configure a game that will start
    void configure_game_mode(game_mode_t mode);
    void configure_map(const char *path);
    void configure_team_count(uint32_t count);
    void configure_team(
        uint32_t team_d,
        team_color_t color,
        uint32_t player_count);

    void start_session();
    void stop_session();

    // Methods to manipulate teams
    void set_teams(
        uint32_t count,
        team_info_t *infos);
    void change_player_team(
        player_t *player,
        team_color_t color);
    void add_player_to_team(
        player_t *player,
        team_color_t color);
    void remove_player_from_team(
        player_t *player);
    bool check_team_joinable(team_color_t color);

    void timestep_begin(float dt_in);
    void timestep_end();

    player_t *add_player();
    void remove_player(uint32_t local_id);
    void clear_players();
    int32_t client_to_local_id(uint16_t client_id);
    // Method makes sure the local id is valid
    player_t *get_player(int32_t local_id);

    void clear_chunks();

    // If chunk doesn't exist, create one
    chunk_t *get_chunk(const ivector3_t &coord);
    // If chunk doesn't exist, return NULL
    chunk_t *access_chunk(const ivector3_t &coord);
    chunk_t **get_active_chunks(uint32_t *count);
    chunk_t **get_modified_chunks(uint32_t *count);
    void reset_modification_tracker();

};

// Equivalent of singleton
extern game_t *g_state;

void allocate_state();

}
