#include "vkph_chunk.hpp"
#include "vkph_constant.hpp"

namespace vkph {

void chunk_t::init(uint32_t chunk_stack_index, const ivector3_t &cchunk_coord) {
    xs_bottom_corner = cchunk_coord * CHUNK_EDGE_LENGTH;
    chunk_coord = cchunk_coord;
    chunk_stack_index = chunk_stack_index;

    flags.made_modification = 0;
    flags.has_to_update_vertices = 0;
    flags.active_vertices = 0;
    flags.modified_marker = 0;
    flags.index_of_modification_struct = 0;

    memset(voxels, 0, sizeof(voxel_t) * CHUNK_EDGE_LENGTH * CHUNK_EDGE_LENGTH * CHUNK_EDGE_LENGTH);

    history.modification_count = 0;
    memset(history.modification_pool, CHUNK_SPECIAL_VALUE, CHUNK_VOXEL_COUNT);

    render = NULL;

    players_in_chunk.init();
}

}
