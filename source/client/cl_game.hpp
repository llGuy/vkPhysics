#pragma once

#include <vkph_state.hpp>

namespace cl {

/*
  "Game" is everything to do with the simulation (players, chunks, etc...)
  but also things like client side prediction, etc...
 */
void init_game(vkph::state_t *state);

/*
  When client receives the information about the server's world in handshake packet,
  this function gets called.
 */
void initialise_client_game_session(uint32_t player_info_count, vkph::player_init_info_t *infos, vkph::state_t *state);

void destroy_game();
void game_input(float dt, vkph::state_t *state, bool is_empty = false);
void tick_game(vkph::state_t *state);
void set_i_am_in_server(bool b);
bool am_i_in_server();
void clear_game(vkph::state_t *state);

}
