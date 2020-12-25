#pragma once

#include <stdint.h>
#include <common/math.hpp>
#include <common/containers.hpp>

#include "vkph_voxel.hpp"
#include "vkph_constant.hpp"

namespace vkph {

struct chunk_history_t {
    /*
      These are all going to be set to 255 by default. If a voxel gets modified, 
      modification_pool[voxel_index] will be set to the initial value of that voxel 
      before modifications.
    */
    uint8_t modification_pool[CHUNK_VOXEL_COUNT];

    int16_t modification_count;
    // Each int16_t is an index into the voxels array of struct chunk_t
    int16_t modification_stack[CHUNK_VOXEL_COUNT / 2];
};

struct chunk_t {
    struct flags_t {
        uint32_t made_modification: 1;
        uint32_t has_to_update_vertices: 1;
        uint32_t active_vertices: 1;
        // Flag that is used temporarily
        uint32_t modified_marker: 1;
        uint32_t index_of_modification_struct: 10;
    } flags;
    
    uint32_t chunk_stack_index;
    ivector3_t xs_bottom_corner;
    ivector3_t chunk_coord;

    voxel_t voxels[CHUNK_VOXEL_COUNT];

    // uint8_t because anyway, player index won't go beyond 50
    static_stack_container_t<uint8_t, PLAYER_MAX_COUNT> players_in_chunk;

    chunk_history_t history;

    struct chunk_render_t *render;

    void init(uint32_t chunk_stack_index, const ivector3_t &chunk_coord);
    void destroy();
};

/*
  Some useful maths functions.
 */
ivector3_t space_world_to_voxel(const vector3_t &ws_position);
ivector3_t space_voxel_to_chunk(const ivector3_t &vs_position);
vector3_t space_chunk_to_world(const ivector3_t &chunk_coord);
ivector3_t space_voxel_to_local_chunk(const ivector3_t &vs_position);
uint32_t get_voxel_index(uint32_t x, uint32_t y, uint32_t z);
uint32_t hash_chunk_coord(const ivector3_t &coord);

}
