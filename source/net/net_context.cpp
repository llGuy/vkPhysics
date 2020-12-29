#include "net_context.hpp"
#include <vkph_chunk.hpp>
#include <vkph_state.hpp>

namespace net {

void context_t::init_main_udp_socket(uint16_t output_port) {
    current_packet = 0;

    main_udp_socket_ = network_socket_init(SP_UDP);
    address_t address = {};
    address.port = host_to_network_byte_order(output_port);
    bind_network_socket_to_port(main_udp_socket_, address);
    set_socket_to_non_blocking_mode(main_udp_socket_);
    set_socket_recv_buffer_size(main_udp_socket_, 1024 * 1024);

    // For debugging purposes
    if (output_port == GAME_OUTPUT_PORT_CLIENT) {
        log_file = fopen("net_log_client.txt", "w+");
    }
    else {
        log_file = fopen("net_log_server.txt", "w+");
    }

    if (!log_file) {
        LOG_INFO("Failed to open log file for networking\n");
    }
}

bool context_t::main_udp_send_to(serialiser_t *serialiser, address_t address) {
    ++current_packet;

    return send_to(main_udp_socket_, address, (char *)serialiser->data_buffer, serialiser->data_buffer_head);
}

int32_t context_t::main_udp_recv_from(char *message_buffer, uint32_t max_size, address_t *addr) {
    return receive_from(main_udp_socket_, message_buffer, sizeof(char) * max_size, addr);
}

void context_t::acc_predicted_modification_init(accumulated_predicted_modification_t *apm_ptr, uint64_t tick) {
    if (!apm_ptr->acc_predicted_modifications) {
        apm_ptr->acc_predicted_modifications =
            (chunk_modifications_t *)chunk_modification_allocator.allocate_arena();
    }

    apm_ptr->acc_predicted_chunk_mod_count = 0;
    apm_ptr->tick = tick;
    memset(
        apm_ptr->acc_predicted_modifications,
        0,
        sizeof(chunk_modifications_t) * NET_MAX_ACCUMULATED_PREDICTED_CHUNK_MODIFICATIONS_PER_PACK);
}

accumulated_predicted_modification_t *context_t::add_acc_predicted_modification() {
    return accumulated_modifications.push_item();
}

void context_t::fill_dummy_voxels(chunk_modifications_t *modifications) {
    for (uint32_t i = 0; i < modifications->modified_voxels_count; ++i) {
        voxel_modification_t *v = &modifications->modifications[i];
        dummy_voxels[v->index] = i;
    }
}

void context_t::unfill_dummy_voxels(chunk_modifications_t *modifications) {
    for (uint32_t i = 0; i < modifications->modified_voxels_count; ++i) {
        voxel_modification_t *v = &modifications->modifications[i];
        dummy_voxels[v->index] = vkph::CHUNK_SPECIAL_VALUE;
    }
}

accumulated_predicted_modification_t *context_t::accumulate_history(const vkph::state_t *state) {
    accumulated_predicted_modification_t *next_acc = add_acc_predicted_modification();
    acc_predicted_modification_init(next_acc, state->current_tick);
    
    next_acc->acc_predicted_chunk_mod_count = fill_chunk_modification_array_with_initial_values(
        next_acc->acc_predicted_modifications, state);

    if (next_acc->acc_predicted_chunk_mod_count == 0) {
        // Pops item that was just pushed
        accumulated_modifications.get_next_item_head();

        return NULL;
    }
    else {
        return next_acc;
    }
}

void context_t::merge_chunk_modifications(
    chunk_modifications_t *dst, uint32_t *dst_count,
    const chunk_modifications_t *src, uint32_t src_count,
    vkph::state_t *state) {
    state->flag_modified_chunks(dst, *dst_count);

    for (uint32_t i = 0; i < src_count; ++i) {
        const chunk_modifications_t *src_modifications = &src[i];

        vkph::chunk_t *chunk = state->get_chunk(ivector3_t(src_modifications->x, src_modifications->y, src_modifications->z));

        // Chunk has been terraformed on before (between previous game state dispatch and next one)
        if (chunk->flags.modified_marker) {
            // Index of modification struct would have been filled by s_flag_modiifed_chunks(), called above;
            chunk_modifications_t *dst_modifications = &dst[chunk->flags.index_of_modification_struct];

            // Has been modified, must fill dummy voxels
            fill_dummy_voxels(dst_modifications);

            for (uint32_t voxel = 0; voxel < src_modifications->modified_voxels_count; ++voxel) {
                const voxel_modification_t *vm_ptr = &src_modifications->modifications[voxel];
                vkph::voxel_color_t color = src_modifications->colors[voxel];

                if (dummy_voxels[vm_ptr->index] == vkph::CHUNK_SPECIAL_VALUE) {
                    // Voxel has not yet been modified, can just push it into array
                    dst_modifications->colors[dst_modifications->modified_voxels_count] = color;
                    dst_modifications->modifications[dst_modifications->modified_voxels_count] = *vm_ptr;
                    ++(dst_modifications->modified_voxels_count);
                }
                else {
                    // Voxel has been modified
                    uint32_t previous_index = dummy_voxels[vm_ptr->index];
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
    
    state->unflag_modified_chunks(dst, *dst_count);
}

}
