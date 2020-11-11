#include "team.hpp"
#include "allocators.hpp"

void team_t::init(
    team_color_t color,
    uint32_t max_player_count,
    uint32_t player_count) {
    color_ = color;
    max_team_player_count_ = max_player_count;
    player_count_ = player_count;
    players_ = FL_MALLOC(uint32_t, max_player_count);
    player_id_to_index_.init();
}

bool team_t::is_full() const {
    return (player_count_ == max_team_player_count_);
}

result_t team_t::add_player(uint32_t player_id) {
    if (player_count_ < max_team_player_count_) {
        // Add the index to this player to the hash map
        player_id_to_index_.insert(player_id, player_count_);

        players_[player_count_++] = player_id;
        return result_t::SUCCESS;
    }
    else {
        return result_t::FAILURE;
    }
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
    return { color_, player_count_, max_team_player_count_ };
}

uint32_t team_t::player_count() const {
    return player_count_;
}
