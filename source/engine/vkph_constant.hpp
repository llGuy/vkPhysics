#pragma once

#include <stdint.h>

#include "vkph_voxel.hpp"

namespace vkph {

constexpr int32_t CHUNK_EDGE_LENGTH = 16;
constexpr uint32_t CHUNK_VOXEL_COUNT = CHUNK_EDGE_LENGTH * CHUNK_EDGE_LENGTH * CHUNK_EDGE_LENGTH;
constexpr uint32_t CHUNK_MAX_LOADED_COUNT = 2000;
constexpr float CHUNK_MAX_VOXEL_VALUE_F = 254.0f;
constexpr uint32_t CHUNK_MAX_VERTICES_PER_CHUNK = 5 * (CHUNK_EDGE_LENGTH - 1) * (CHUNK_EDGE_LENGTH - 1) * (CHUNK_EDGE_LENGTH - 1);
constexpr uint32_t CHUNK_MAX_VOXEL_VALUE_I = 254;
constexpr uint32_t CHUNK_SPECIAL_VALUE = 255;
constexpr uint32_t CHUNK_SURFACE_LEVEL = 70;
constexpr uint32_t CHUNK_BYTE_SIZE = CHUNK_VOXEL_COUNT * sizeof(voxel_t);

constexpr uint32_t PLAYER_MAX_COUNT = 50;
constexpr float PLAYER_SHAPE_SWITCH_DURATION = 0.3f;
constexpr uint32_t PLAYER_MAX_ACTIONS_COUNT = 100;
constexpr float PLAYER_SCALE = 0.5f;
constexpr float PLAYER_TERRAFORMING_SPEED = 200.0f;
constexpr float PLAYER_TERRAFORMING_RADIUS = 3.0f;
constexpr float PLAYER_WALKING_SPEED = 25.0f;

constexpr uint32_t PROJECTILE_MAX_ROCK_COUNT = 1000;
constexpr float PROJECTILE_ROCK_SPEED = 35.0f;

constexpr float GRAVITY_ACCELERATION = 10.0f;
constexpr float SHAPE_SWITCH_ANIMATION_TIME = 0.3f;

}
