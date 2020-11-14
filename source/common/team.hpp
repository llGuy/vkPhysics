#pragma once

#include <bits/stdint-uintn.h>
#include <stdint.h>

#include "common/containers.hpp"
#include "tools.hpp"

enum team_color_t { INVALID, BLUE, RED, YELLOW, GREEN, PURPLE, COUNT };

struct team_info_t {
    team_color_t color;
    uint32_t player_count;
    uint32_t max_players;
};

class team_t {
public:
    team_t() = default;

    void init(
        team_color_t color,
        uint32_t max_player_count,
        uint32_t player_count);

    result_t add_player(uint32_t player_id);
    void remove_player(uint32_t player_id);

    bool is_full() const;
    team_info_t make_team_info() const;
    uint32_t player_count() const;
private:
    hash_table_t<uint32_t, 30, 5, 5> player_id_to_index_;
    stack_container_t<uint32_t> players_;
    uint32_t player_count_;
    team_color_t color_;
};

const char *team_color_to_string(team_color_t color);
