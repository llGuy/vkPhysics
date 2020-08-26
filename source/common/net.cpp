#include "net.hpp"
#include "game.hpp"
#include "string.hpp"
#include "socket.hpp"
#include "meta_packet.hpp"
#include <cstdio>

net_data_t g_net_data = {};

static socket_t meta_socket;
static socket_t main_udp_socket;

void main_udp_socket_init(uint16_t output_port) {
    g_net_data.current_packet = 0;

    main_udp_socket = network_socket_init(SP_UDP);
    network_address_t address = {};
    address.port = host_to_network_byte_order(output_port);
    bind_network_socket_to_port(main_udp_socket, address);
    set_socket_to_non_blocking_mode(main_udp_socket);
    set_socket_recv_buffer_size(main_udp_socket, 1024 * 1024);

    // For debugging purposes
    if (output_port == GAME_OUTPUT_PORT_CLIENT) {
        g_net_data.log_file = fopen("net_log_client.txt", "w+");
    }
    else {
        g_net_data.log_file = fopen("net_log_server.txt", "w+");
    }

    if (!g_net_data.log_file) {
        LOG_INFO("Failed to open log file for networking\n");
    }
}

#define META_SERVER_DOMAIN "www.llguy.fun"

void meta_socket_init() {
    meta_socket = network_socket_init(SP_TCP);
    connect_to_address(meta_socket, META_SERVER_DOMAIN, SERVER_META_OUTPUT_PORT, SP_TCP);
    set_socket_to_non_blocking_mode(meta_socket);
}

bool send_to_game_server(
    serialiser_t *serialiser,
    network_address_t address) {
    ++g_net_data.current_packet;
    return send_to(main_udp_socket, address, (char *)serialiser->data_buffer, serialiser->data_buffer_head);
}

bool send_to_client(serialiser_t *serialiser, network_address_t address) {
    // Check that we need this
    ++g_net_data.current_packet;
    return send_to(main_udp_socket, address, (char *)serialiser->data_buffer, serialiser->data_buffer_head);
}

int32_t receive_from_game_server(char *message_buffer, uint32_t max_size, network_address_t *addr) {
    return receive_from(
        main_udp_socket,
        message_buffer,
        sizeof(char) * max_size,
        addr);
}

int32_t receive_from_client(char *message_buffer, uint32_t max_size, network_address_t *addr) {
    return receive_from(
        main_udp_socket,
        message_buffer,
        sizeof(char) * max_size,
        addr);
}

bool send_to_meta_server(
    serialiser_t *serialiser) {
    return send_to_bound_address(meta_socket, (char *)serialiser->data_buffer, serialiser->data_buffer_head);
}

void acc_predicted_modification_init(accumulated_predicted_modification_t *apm_ptr, uint64_t tick) {
    if (!apm_ptr->acc_predicted_modifications) {
        apm_ptr->acc_predicted_modifications = (chunk_modifications_t *)g_net_data.chunk_modification_allocator.allocate_arena();
    }

    apm_ptr->acc_predicted_chunk_mod_count = 0;
    apm_ptr->tick = tick;
    memset(
        apm_ptr->acc_predicted_modifications,
        0,
        sizeof(chunk_modifications_t) * NET_MAX_ACCUMULATED_PREDICTED_CHUNK_MODIFICATIONS_PER_PACK);
}

accumulated_predicted_modification_t *add_acc_predicted_modification() {
    return g_net_data.acc_predicted_modifications.push_item();
}

// HPT_RESPONSE_AVAILABLE_SERVERS
static void s_receive_response_available_servers(
    serialiser_t *serialiser,
    event_submissions_t *submission) {
    g_net_data.available_servers.name_to_server.clear();
    
    meta_response_available_servers_t response = {};
    deserialise_meta_response_available_servers(&response, serialiser);

    LOG_INFOV("There are %i available servers\n", response.server_count);

    g_net_data.available_servers.server_count = response.server_count;
    for (uint32_t i = 0; i < response.server_count; ++i) {
        meta_server_info_t *src = &response.servers[i];
        game_server_t *dst = &g_net_data.available_servers.servers[i];

        if (dst->server_name) {
            FL_FREE((void *)dst->server_name);
        }
        
        dst->ipv4_address = src->ipv4_address;
        dst->server_name = create_fl_string(src->server_name);
        
        g_net_data.available_servers.name_to_server.insert(simple_string_hash(dst->server_name), i);
    }

    submit_event(ET_RECEIVED_AVAILABLE_SERVERS, NULL, submission);
}

void check_incoming_meta_server_packets(event_submissions_t *events) {
    int32_t received = receive_from_bound_address(
        meta_socket,
        g_net_data.message_buffer,
        sizeof(char) * NET_MAX_MESSAGE_SIZE);

    // In future, separate thread will be capturing all these packets
    static const uint32_t MAX_RECEIVED_PER_TICK = 4;
    uint32_t i = 0;

    while (received) {
        serialiser_t in_serialiser = {};
        in_serialiser.data_buffer = (uint8_t *)g_net_data.message_buffer;
        in_serialiser.data_buffer_size = received;

        meta_packet_header_t header = {};
        deserialise_meta_packet_header(&header, &in_serialiser);

        switch (header.type) {
        case HPT_RESPONSE_AVAILABLE_SERVERS: {
            s_receive_response_available_servers(&in_serialiser, events);
            break;
        }
        case HPT_QUERY_RESPONSIVENESS: {
            // HPT_QUERY_RESPONSIVENESS
            meta_packet_header_t new_header = {};
            new_header.type = HPT_RESPONSE_RESPONSIVENESS;

            serialiser_t serialiser = {};
            serialiser.init(20);

            serialise_meta_packet_header(&new_header, &serialiser);

            send_to_bound_address(meta_socket, (char *)serialiser.data_buffer, serialiser.data_buffer_head);
            break;
        }
        default: {
            break;
        }
        }

        if (i < MAX_RECEIVED_PER_TICK) {
            received = receive_from_bound_address(
                meta_socket,
                g_net_data.message_buffer,
                sizeof(char) * NET_MAX_MESSAGE_SIZE);
        }
        else {
            received = false;
        } 

        ++i;
    }
}

void flag_modified_chunks(
    chunk_modifications_t *modifications,
    uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
        chunk_modifications_t *m_ptr = &modifications[i];
        chunk_t *c_ptr = get_chunk(ivector3_t(m_ptr->x, m_ptr->y, m_ptr->z));
        c_ptr->flags.modified_marker = 1;
        c_ptr->flags.index_of_modification_struct = i;
    }
}

void unflag_modified_chunks(
    chunk_modifications_t *modifications,
    uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
        chunk_modifications_t *m_ptr = &modifications[i];
        chunk_t *c_ptr = get_chunk(ivector3_t(m_ptr->x, m_ptr->y, m_ptr->z));
        c_ptr->flags.modified_marker = 0;
        c_ptr->flags.index_of_modification_struct = 0;
    }
}

void fill_dummy_voxels(
    chunk_modifications_t *modifications) {
    for (uint32_t i = 0; i < modifications->modified_voxels_count; ++i) {
        voxel_modification_t *v = &modifications->modifications[i];
        g_net_data.dummy_voxels[v->index] = i;
    }
}

void unfill_dummy_voxels(
    chunk_modifications_t *modifications) {
    for (uint32_t i = 0; i < modifications->modified_voxels_count; ++i) {
        voxel_modification_t *v = &modifications->modifications[i];
        g_net_data.dummy_voxels[v->index] = CHUNK_SPECIAL_VALUE;
    }
}


uint32_t fill_chunk_modification_array(
    chunk_modifications_t *modifications) {
    uint32_t modified_chunk_count = 0;
    chunk_t **chunks = get_modified_chunks(&modified_chunk_count);

    uint32_t current = 0;
            
    for (uint32_t c_index = 0; c_index < modified_chunk_count; ++c_index) {
        chunk_modifications_t *cm_ptr = &modifications[current];
        chunk_t *c_ptr = chunks[c_index];
        chunk_history_t *h_ptr = &chunks[c_index]->history;

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
                cm_ptr->modifications[v_index].initial_value = (uint8_t)h_ptr->modification_pool[cm_ptr->modifications[v_index].index];
                cm_ptr->modifications[v_index].final_value = (uint8_t)c_ptr->voxels[cm_ptr->modifications[v_index].index];
            }

            ++current;
        }
    }

    return current;
}

accumulated_predicted_modification_t *accumulate_history() {
    accumulated_predicted_modification_t *next_acc = add_acc_predicted_modification();
    acc_predicted_modification_init(next_acc, get_current_tick());
    
    next_acc->acc_predicted_chunk_mod_count = fill_chunk_modification_array(next_acc->acc_predicted_modifications);

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
    uint32_t src_count) {
    flag_modified_chunks(dst, *dst_count);

    for (uint32_t i = 0; i < src_count; ++i) {
        chunk_modifications_t *src_modifications = &src[i];

        chunk_t *chunk = get_chunk(ivector3_t(src_modifications->x, src_modifications->y, src_modifications->z));

        // Chunk has been terraformed on before (between previous game state dispatch and next one)
        if (chunk->flags.modified_marker) {
            // Index of modification struct would have been filled by s_flag_modiifed_chunks(), called above;
            chunk_modifications_t *dst_modifications = &dst[chunk->flags.index_of_modification_struct];

            // Has been modified, must fill dummy voxels
            fill_dummy_voxels(dst_modifications);

            for (uint32_t voxel = 0; voxel < src_modifications->modified_voxels_count; ++voxel) {
                voxel_modification_t *vm_ptr = &src_modifications->modifications[voxel];

                if (g_net_data.dummy_voxels[vm_ptr->index] == CHUNK_SPECIAL_VALUE) {
                    // Voxel has not yet been modified, can just push it into array
                    dst_modifications->modifications[dst_modifications->modified_voxels_count++] = *vm_ptr;
                }
                else {
                    // Voxel has been modified
                    uint32_t previous_index = g_net_data.dummy_voxels[vm_ptr->index];
                    // Just update final value
                    dst_modifications->modifications[previous_index].final_value = vm_ptr->final_value;
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
        }
    }
    
    unflag_modified_chunks(dst, *dst_count);
}
