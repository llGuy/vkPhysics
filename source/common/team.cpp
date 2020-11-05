#include "team.hpp"
#include "allocators.hpp"

void team_t::init(
    team_color_t color,
    uint32_t max_player_count) {
    color_ = color;
    max_team_player_count_ = max_player_count;
    player_count_ = 0;
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
