#pragma once

#include <bits/stdint-uintn.h>
#include <stdint.h>

#include "common/containers.hpp"
#include "tools.hpp"

enum team_color_t { BLUE, RED, YELLOW, GREEN, PURPLE };

class team_t {
public:
    team_t() = default;

    void init(
        team_color_t color,
        uint32_t max_player_count);

    bool is_full() const;
    result_t add_player(uint32_t player_id);
private:
    hash_table_t<uint32_t, 30, 5, 5> player_id_to_index_;
    team_color_t color_;
    uint32_t max_team_player_count_;
    uint32_t player_count_;
    uint32_t *players_;
};
