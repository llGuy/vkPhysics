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
    return acc_predicted_modifications.push_item();
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

uint32_t fill_chunk_modification_array_with_initial_values(chunk_modifications_t *modifications, vkph::state_t *state) {
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
                cm_ptr->modifications[v_index].initial_value = h_ptr->modification_pool[cm_ptr->modifications[v_index].index];
                cm_ptr->modifications[v_index].final_value = c_ptr->voxels[cm_ptr->modifications[v_index].index].value;
                cm_ptr->colors[v_index] = c_ptr->voxels[cm_ptr->modifications[v_index].index].color;
            }

            ++current;
        }
    }

    return current;
}

}
