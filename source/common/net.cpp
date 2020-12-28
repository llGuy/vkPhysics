#include "net.hpp"
#include "allocators.hpp"
#include "common/constant.hpp"
#include "game_packet.hpp"
#include <vkph_state.hpp>
#include "string.hpp"
#include "socket.hpp"
#include "meta_packet.hpp"
#include <cstdio>
#include <mutex>
#include <thread>
#include <atomic>

#include <vkph_chunk.hpp>
#include <vkph_events.hpp>

uint32_t fill_chunk_modification_array_with_colors(chunk_modifications_t *modifications, vkph::state_t *state) {
    uint32_t modified_chunk_count = 0;
    vkph::chunk_t **chunks = state->get_modified_chunks(&modified_chunk_count);

    uint32_t current = 0;
            
    for (uint32_t c_index = 0; c_index < modified_chunk_count; ++c_index) {
        chunk_modifications_t *cm_ptr = &modifications[current];
        vkph::chunk_t *c_ptr = chunks[c_index];
        vkph::chunk_history_t *h_ptr = &chunks[c_index]->history;

        if (h_ptr->modification_count == 0) {
            // Chunk doesn't actually have modifications, it was just flagged
        }
        else {
            cm_ptr->x = c_ptr->chunk_coord.x;
            cm_ptr->y = c_ptr->chunk_coord.y;
            cm_ptr->z = c_ptr->chunk_coord.z;
            cm_ptr->modified_voxels_count = h_ptr->modification_count;
            for (uint32_t v_index = 0; v_index < cm_ptr->modified_voxels_count; ++v_index) {
                cm_ptr->modifications[v_index].index = (uint16_t)h_ptr->modification_stack[v_index];
                // Difference is here because the server will just send the voxel values array, not the colors array
                cm_ptr->modifications[v_index].color = c_ptr->voxels[cm_ptr->modifications[v_index].index].color;
                cm_ptr->modifications[v_index].final_value = c_ptr->voxels[cm_ptr->modifications[v_index].index].value;
                cm_ptr->colors[v_index] = c_ptr->voxels[cm_ptr->modifications[v_index].index].color;
            }

            ++current;
        }
    }

    return current;
}

accumulated_predicted_modification_t *accumulate_history(vkph::state_t *state) {
    accumulated_predicted_modification_t *next_acc = add_acc_predicted_modification();
    acc_predicted_modification_init(next_acc, state->current_tick);
    
    next_acc->acc_predicted_chunk_mod_count = fill_chunk_modification_array_with_initial_values(next_acc->acc_predicted_modifications, state);

    if (next_acc->acc_predicted_chunk_mod_count == 0) {
        // Pops item that was just pushed
        g_net_data.acc_predicted_modifications.get_next_item_head();

        return NULL;
    }
    else {
        return next_acc;
    }
}

void merge_chunk_modifications(
    chunk_modifications_t *dst,
    uint32_t *dst_count,
    chunk_modifications_t *src,
    uint32_t src_count,
    vkph::state_t *state) {
    flag_modified_chunks(dst, *dst_count, state);

    for (uint32_t i = 0; i < src_count; ++i) {
        chunk_modifications_t *src_modifications = &src[i];

        vkph::chunk_t *chunk = state->get_chunk(ivector3_t(src_modifications->x, src_modifications->y, src_modifications->z));

        // Chunk has been terraformed on before (between previous game state dispatch and next one)
        if (chunk->flags.modified_marker) {
            // Index of modification struct would have been filled by s_flag_modiifed_chunks(), called above;
            chunk_modifications_t *dst_modifications = &dst[chunk->flags.index_of_modification_struct];

            // Has been modified, must fill dummy voxels
            fill_dummy_voxels(dst_modifications);

            for (uint32_t voxel = 0; voxel < src_modifications->modified_voxels_count; ++voxel) {
                voxel_modification_t *vm_ptr = &src_modifications->modifications[voxel];
                vkph::voxel_color_t color = src_modifications->colors[voxel];

                if (g_net_data.dummy_voxels[vm_ptr->index] == vkph::CHUNK_SPECIAL_VALUE) {
                    // Voxel has not yet been modified, can just push it into array
                    dst_modifications->colors[dst_modifications->modified_voxels_count] = color;
                    dst_modifications->modifications[dst_modifications->modified_voxels_count] = *vm_ptr;
                    ++(dst_modifications->modified_voxels_count);
                }
                else {
                    // Voxel has been modified
                    uint32_t previous_index = g_net_data.dummy_voxels[vm_ptr->index];
                    // Just update final value
                    dst_modifications->modifications[previous_index].final_value = vm_ptr->final_value;
                    dst_modifications->colors[previous_index] = color;
                }
            }

            unfill_dummy_voxels(dst_modifications);
        }
        else {
            // Chunk has not been terraformed before, need to push a new modification
            chunk_modifications_t *m = &dst[*(dst_count)];
            ++(*dst_count);

            m->x = src_modifications->x;
            m->y = src_modifications->y;
            m->z = src_modifications->z;

            m->modified_voxels_count = src_modifications->modified_voxels_count;
            memcpy(m->modifications, src_modifications->modifications, sizeof(voxel_modification_t) * m->modified_voxels_count);
            memcpy(m->colors, src_modifications->colors, sizeof(vkph::voxel_color_t) * m->modified_voxels_count);
        }
    }
    
    unflag_modified_chunks(dst, *dst_count, state);
}
