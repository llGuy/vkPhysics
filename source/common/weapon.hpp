#pragma once

#include "tools.hpp"
#include "containers.hpp"

#include <utility>


// Initialise all the memory to contain the rocks / bullets which will deal damage
void weapons_and_bullets_memory_init();

bool check_projectile_players_collision(rock_t *rock, int32_t *dst_player);
bool check_projectile_terrain_collision(rock_t *rock);

