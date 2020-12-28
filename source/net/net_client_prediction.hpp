#pragma once

#include <common/math.hpp>
#include "net_chunk_tracker.hpp"
#include <vkph_player.hpp>

namespace net {

/*
  Everytime the client sends the client commands packet, the predicted
  state will also be sent. We need to make sure to keep track of this
  predicted state so that the server can check if the predictions were incorrect.
 */
struct client_prediction_t {
    vector3_t ws_position;
    vector3_t ws_view_direction;
    vector3_t ws_up_vector;
    vector3_t ws_velocity;
    vkph::player_flags_t player_flags;
    uint32_t player_health;
    uint32_t chunk_mod_count;
    chunk_modifications_t *chunk_modifications;
};


}
