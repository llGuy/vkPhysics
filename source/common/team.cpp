#include "team.hpp"
#include "allocators.hpp"

void team_t::init(
    team_color_t color,
    uint32_t max_player_count,
    uint32_t player_count) {
    color_ = color;
    players_.init(max_player_count);
    player_id_to_index_.init();
    player_count_ = 0;
}

bool team_t::is_full() const {
    return (player_count_ == players_.max_size);
}

result_t team_t::add_player(uint32_t player_id) {
    if (player_count_ < players_.max_size) {
        uint32_t idx = players_.add();

        // Add the index to this player to the hash map
        player_id_to_index_.insert(player_id, idx);

        *players_.get(idx) = player_id;

        player_count_++;

        return result_t::SUCCESS;
    }
    else {
        return result_t::FAILURE;
    }
}

void team_t::remove_player(uint32_t player_id) {
    uint32_t *idx = player_id_to_index_.get(player_id);
    players_.remove(*idx);
    player_count_--;

    // Remove from hash map
    player_id_to_index_.remove(player_id);
}

const char *team_color_to_string(team_color_t color) {
    switch (color) {
    case team_color_t::BLUE: {
        return "BLUE";
    } break;

    case team_color_t::RED: {
        return "RED";
    } break;

    case team_color_t::GREEN: {
        return "GREEN";
    } break;

    case team_color_t::PURPLE: {
        return "PURPLE";
    } break;

    case team_color_t::YELLOW: {
        return "YELLOW";
    } break;
    }
}

team_info_t team_t::make_team_info() const {
    return { color_, player_count_, players_.max_size };
}

uint32_t team_t::player_count() const {
    return player_count_;
}
