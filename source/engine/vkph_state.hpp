#pragma once

#include "vkph_map.hpp"
#include "vkph_team.hpp"
#include "vkph_player.hpp"
#include "vkph_weapon.hpp"
#include "vkph_constant.hpp"
#include "vkph_terraform.hpp"
#include "vkph_projectile.hpp"
#include "vkph_projectile_tracker.hpp"

#include <stdint.h>
#include <common/files.hpp>
#include <common/containers.hpp>

namespace vkph {

enum class game_mode_t { DEATHMATCH, CAPTURE_THE_FLAG, INVALID };

struct chunk_t;

/*
  Most of these members need to be public simply because the
  client and server code need to have a lot of control over this
  structure.
*/
struct state_t {

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

    // Maps ///////////////////////////////////////////////////////////////////
    map_names_t map_names;


    // Methods ////////////////////////////////////////////////////////////////
    // Doesn't actually start a game - just allocates the necessary memory, etc...
    void prepare();

    // Configure a game that will start (for the server)
    void configure_game_mode(game_mode_t mode);
    void configure_map(const char *path);
    void configure_team_count(uint32_t count);
    void configure_team(uint32_t team_d, team_color_t color, uint32_t player_count);

    void start_session();
    void stop_session();

    // Methods to manipulate teams
    void set_teams(uint32_t count, team_info_t *infos);
    void change_player_team(player_t *player, team_color_t color);
    void add_player_to_team(player_t *player, team_color_t color);
    void remove_player_from_team(player_t *player);
    bool check_team_joinable(team_color_t color) const;

    void timestep_begin(float dt_in);
    void timestep_end();

    player_t *add_player();
    void remove_player(uint32_t local_id);
    void clear_players();
    int32_t get_local_id(uint16_t client_id) const;
    // Method makes sure the local id is valid
    player_t *get_player(int32_t local_id);
    const player_t *get_player(int32_t local_id) const;

    void clear_chunks();

    // If chunk doesn't exist, create one
    chunk_t *get_chunk(const ivector3_t &coord);
    // If chunk doesn't exist, return NULL
    chunk_t *access_chunk(const ivector3_t &coord);
    const chunk_t *access_chunk(const ivector3_t &coord) const;
    chunk_t **get_active_chunks(uint32_t *count);
    const chunk_t **get_active_chunks(uint32_t *count) const;
    chunk_t **get_modified_chunks(uint32_t *count);
    void reset_modification_tracker();

    // Some terrain modification stuff
    void generate_sphere(sphere_create_info_t *info);
    void generate_hollow_sphere(sphere_create_info_t *info);
    void generate_platform(platform_create_info_t *info);
    void generate_math_equation(math_equation_create_info_t *info);
    bool terraform(terraform_info_t *info);

    map_t *load_map(const char *path);
    void save_map(map_t *map = NULL);
    void add_map_name(const char *map_name, const char *path);

private:

    file_handle_t map_names_file_;
    hash_table_t<uint32_t, 10, 5, 5> map_name_to_index_;


    void load_map_names();
    bool terraform_with_history(terraform_info_t *info);
    bool terraform_without_history(terraform_info_t *info);

};

void allocate_state();

}
