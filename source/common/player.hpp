#pragma once

#include "math.hpp"
#include "event.hpp"
#include "chunk.hpp"
#include "tools.hpp"
#include "weapon.hpp"
#include "constant.hpp"
#include "containers.hpp"

// If false was returned, the player died
void execute_action(player_t *player, player_action_t *action);
void handle_shape_switch(player_t *p, bool switch_shapes, float dt);

bool collide_sphere_with_standing_player(const vector3_t &target, const vector3_t &up, const vector3_t &center, float radius);
bool collide_sphere_with_rolling_player(const vector3_t &target, const vector3_t &center, float radius);
bool collide_sphere_with_player(const player_t *p, const vector3_t &center, float radius);

// Make sure chunk has correct information as to where each player is
void update_player_chunk_status(player_t *p);
