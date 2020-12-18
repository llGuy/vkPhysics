#include "net.hpp"
#include "allocators.hpp"
#include "common/constant.hpp"
#include "game_packet.hpp"
#include "game.hpp"
#include "string.hpp"
#include "socket.hpp"
#include "meta_packet.hpp"
#include <bits/stdint-intn.h>
#include <cstdio>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>

net_data_t g_net_data = {};

static socket_t meta_socket;
static socket_t main_udp_socket;

// Don't need to udp thread on server because server tick rate doesn't
// depend so heavily on framerate (which is a huge burden on client side)
static std::thread client_udp_thread;
static std::mutex udp_thread_mutex;
static std::atomic<bool> udp_thread_running;

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

// If the packet is a ping packet, return true
static bool s_handle_ping(const packet_t &packet) {
    // Do quick check to see if it's ping, in which case, we send something back
    serialiser_t ser = {};
    ser.data_buffer = (uint8_t *)packet.data;
    ser.data_buffer_size = packet.byte_size;

    packet_header_t header = {};
    deserialise_packet_header(&header, &ser);

    if (header.flags.packet_type == PT_PING) {
        LOG_INFO("Received ping\n");

        // Send pong
        send_to(main_udp_socket, packet.address, (char *)ser.data_buffer, ser.data_buffer_head);

        return true;
    }

    return false;
}

static void s_client_udp_thread() {
    while (udp_thread_running) {
        std::lock_guard<std::mutex> guard (udp_thread_mutex);

        // Get the starting pointer
        packet_t packet = {};
        packet.data = g_net_data.message_buffer;
        if (g_net_data.current_packet_count) {
            packet_t *previous_packet = &g_net_data.packets[g_net_data.current_packet_count - 1];
            packet.data = (char *)(previous_packet->data) + previous_packet->byte_size;
        }

        if (g_net_data.current_packet_count < MAX_CLIENT_PACKET_STORAGE) {
            packet.byte_size = receive_from(
                main_udp_socket,
                (char *)packet.data,
                sizeof(char) * NET_MAX_MESSAGE_SIZE,
                &packet.address);
        
            if (packet.byte_size > 0) {
                // If this packet isn't a ping packet, make sure not to push
                if (!s_handle_ping(packet)) {
                    // Push this packet to the list of packets
                    LOG_INFO("Received packet that wasn't ping\n");
                    g_net_data.packets[g_net_data.current_packet_count++] = packet;
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::duration<float>(0.00001f));
    }
}

void start_client_udp_thread() {
    udp_thread_running = true;
    client_udp_thread = std::thread(s_client_udp_thread);
}

void stop_client_udp_thread() {
    udp_thread_running = false;
    client_udp_thread.join();

    LOG_INFO("UDP thread finished\n");
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

    std::lock_guard<std::mutex> guard (udp_thread_mutex);

    LOG_INFO("Sending packet to server\n");

    return send_to(main_udp_socket, address, (char *)serialiser->data_buffer, serialiser->data_buffer_head);
}

bool send_to_client(serialiser_t *serialiser, network_address_t address) {
    // Check that we need this
    ++g_net_data.current_packet;
    return send_to(main_udp_socket, address, (char *)serialiser->data_buffer, serialiser->data_buffer_head);
}

std::unique_lock<std::mutex> get_next_received_packet(packet_t **p) {
    *p = NULL;

    // Make sure that this gets locked until we have fully handled this packet
    std::unique_lock<std::mutex> lock (udp_thread_mutex);

    if (g_net_data.packet_pointer < g_net_data.current_packet_count) {
        packet_t *p = &g_net_data.packets[g_net_data.packet_pointer];

        ++g_net_data.packet_pointer;
        if (g_net_data.current_packet_count == g_net_data.packet_pointer) {
            g_net_data.current_packet_count = 0;
            g_net_data.packet_pointer = 0;
        }
    }

    return lock;
}

// int32_t receive_from_game_server(char *message_buffer, uint32_t max_size, network_address_t *addr) {
//     std::lock_guard<std::mutex> guard (udp_thread_mutex);

//     uint32_t byte_size = 0;

//     if (g_net_data.packet_pointer < g_net_data.current_packet_count) {
//         packet_t *p = &g_net_data.packets[g_net_data.packet_pointer];

//         byte_size = p->byte_size;

//         ++g_net_data.packet_pointer;
//         if (g_net_data.current_packet_count == g_net_data.packet_pointer) {
//             g_net_data.current_packet_count = 0;
//             g_net_data.packet_pointer = 0;
//         }
//     }

//     return byte_size;
// }

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
        apm_ptr->acc_predicted_modifications =
            (chunk_modifications_t *)g_net_data.chunk_modification_allocator.allocate_arena();
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
        chunk_t *c_ptr = g_game->get_chunk(ivector3_t(m_ptr->x, m_ptr->y, m_ptr->z));
        c_ptr->flags.modified_marker = 1;
        c_ptr->flags.index_of_modification_struct = i;
    }
}

void unflag_modified_chunks(
    chunk_modifications_t *modifications,
    uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
        chunk_modifications_t *m_ptr = &modifications[i];
        chunk_t *c_ptr = g_game->get_chunk(ivector3_t(m_ptr->x, m_ptr->y, m_ptr->z));
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


uint32_t fill_chunk_modification_array_with_initial_values(
    chunk_modifications_t *modifications) {
    uint32_t modified_chunk_count = 0;
    chunk_t **chunks = g_game->get_modified_chunks(&modified_chunk_count);

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
                cm_ptr->modifications[v_index].initial_value = h_ptr->modification_pool[cm_ptr->modifications[v_index].index];
                cm_ptr->modifications[v_index].final_value = c_ptr->voxels[cm_ptr->modifications[v_index].index].value;
                cm_ptr->colors[v_index] = c_ptr->voxels[cm_ptr->modifications[v_index].index].color;
            }

            ++current;
        }
    }

    return current;
}

uint32_t fill_chunk_modification_array_with_colors(chunk_modifications_t *modifications) {
    uint32_t modified_chunk_count = 0;
    chunk_t **chunks = g_game->get_modified_chunks(&modified_chunk_count);

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

accumulated_predicted_modification_t *accumulate_history() {
    accumulated_predicted_modification_t *next_acc = add_acc_predicted_modification();
    acc_predicted_modification_init(next_acc, g_game->current_tick);
    
    next_acc->acc_predicted_chunk_mod_count = fill_chunk_modification_array_with_initial_values(next_acc->acc_predicted_modifications);

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

        chunk_t *chunk = g_game->get_chunk(ivector3_t(src_modifications->x, src_modifications->y, src_modifications->z));

        // Chunk has been terraformed on before (between previous game state dispatch and next one)
        if (chunk->flags.modified_marker) {
            // Index of modification struct would have been filled by s_flag_modiifed_chunks(), called above;
            chunk_modifications_t *dst_modifications = &dst[chunk->flags.index_of_modification_struct];

            // Has been modified, must fill dummy voxels
            fill_dummy_voxels(dst_modifications);

            for (uint32_t voxel = 0; voxel < src_modifications->modified_voxels_count; ++voxel) {
                voxel_modification_t *vm_ptr = &src_modifications->modifications[voxel];
                voxel_color_t color = src_modifications->colors[voxel];

                if (g_net_data.dummy_voxels[vm_ptr->index] == CHUNK_SPECIAL_VALUE) {
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
            memcpy(m->colors, src_modifications->colors, sizeof(voxel_color_t) * m->modified_voxels_count);
        }
    }
    
    unflag_modified_chunks(dst, *dst_count);
}
