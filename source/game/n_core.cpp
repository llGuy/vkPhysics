#include "net.hpp"
#include "world.hpp"
#include "engine.hpp"
#include "n_internal.hpp"
#include <common/log.hpp>
#include <common/event.hpp>
#include <common/string.hpp>
#include <common/containers.hpp>
#include <common/allocators.hpp>
#include <glm/gtx/string_cast.hpp>

static float client_command_output_interval = 1.0f / 25.0f;
static float server_snapshot_output_interval = 1.0f / 20.0f;

#define MAX_MESSAGE_SIZE 65507

// Socket that connects to hub
static socket_t hub_socket;

static char *message_buffer;
static socket_t main_udp_socket;
static uint64_t current_packet;

static void s_main_udp_socket_init(
    uint16_t output_port) {
    current_packet = 0;

    main_udp_socket = network_socket_init(SP_UDP);
    network_address_t address = {};
    address.port = host_to_network_byte_order(output_port);
    bind_network_socket_to_port(main_udp_socket, address);
    set_socket_to_non_blocking_mode(main_udp_socket);
    set_socket_recv_buffer_size(main_udp_socket, 1024 * 1024);
}

static void s_hub_socket_init() {
    hub_socket = network_socket_init(SP_TCP);
    network_address_t address = {};
    address.port = host_to_network_byte_order(SERVER_HUB_OUTPUT_PORT);
    // This will be hardcoded for now
    address.ipv4_address = str_to_ipv4_int32("127.0.0.1", SERVER_HUB_OUTPUT_PORT, SP_TCP);
    connect_to_address(hub_socket, address);
}

static bool s_send_to(
    serialiser_t *serialiser,
    network_address_t address) {
    ++current_packet;
    return send_to(main_udp_socket, address, (char *)serialiser->data_buffer, serialiser->data_buffer_head);
}

static bool started_client = 0;
static uint16_t current_client_id;
static bool client_check_incoming_packets = 0;

#define MAX_CLIENTS 50
static stack_container_t<client_t> clients;

static uint8_t dummy_voxels[CHUNK_VOXEL_COUNT];

#define MAX_ACCUMULATED_PREDICTED_CHUNK_MODIFICATIONS_PACK_COUNT 60
#define MAX_ACCUMULATED_PREDICTED_CHUNK_MODIFICATIONS_PER_PACK MAX_PREDICTED_CHUNK_MODIFICATIONS * 5

struct accumulated_predicted_modification_t {
    // Tick at which client sent these to the server
    uint64_t tick;
    // Accumulated predicted chunk modifications
    uint32_t acc_predicted_chunk_mod_count;
    // These will stay until server returns with game state dispatch confirming no errors have happened
    chunk_modifications_t *acc_predicted_modifications;
};

static arena_allocator_t chunk_modification_allocator;

// BIGGG array
// TODO: Need to replace this with a circular buffer
static circular_buffer_array_t<accumulated_predicted_modification_t, MAX_ACCUMULATED_PREDICTED_CHUNK_MODIFICATIONS_PACK_COUNT> acc_predicted_modifications;
// These will contain ALL recent modifications (merged), above does not merge them
static accumulated_predicted_modification_t merged_recent_modifications;

static void s_acc_predicted_modification_init(
    accumulated_predicted_modification_t *apm_ptr,
    uint64_t tick) {
    if (!apm_ptr->acc_predicted_modifications) {
        apm_ptr->acc_predicted_modifications = (chunk_modifications_t *)chunk_modification_allocator.allocate_arena();
    }

    apm_ptr->acc_predicted_chunk_mod_count = 0;
    apm_ptr->tick = tick;
    memset(apm_ptr->acc_predicted_modifications, 0, sizeof(chunk_modifications_t) * MAX_ACCUMULATED_PREDICTED_CHUNK_MODIFICATIONS_PER_PACK);
}

static accumulated_predicted_modification_t *s_add_acc_predicted_modification() {
    accumulated_predicted_modification_t *apm_ptr = acc_predicted_modifications.push_item();

    return apm_ptr;
}

static void s_start_client() {
    memset(dummy_voxels, SPECIAL_VALUE, sizeof(dummy_voxels));

    s_main_udp_socket_init(GAME_OUTPUT_PORT_CLIENT);

    clients.init(MAX_CLIENTS);
    
    started_client = 1;

    set_chunk_history_tracker_value(1);

    acc_predicted_modifications.init();

    uint32_t sizeof_chunk_mod_pack = sizeof(chunk_modifications_t) * MAX_PREDICTED_CHUNK_MODIFICATIONS;

    chunk_modification_allocator.pool_init(
        sizeof_chunk_mod_pack,
        sizeof_chunk_mod_pack * (MAX_ACCUMULATED_PREDICTED_CHUNK_MODIFICATIONS_PER_PACK + 4));

    //s_acc_predicted_modification_init(&merged_recent_modifications, 0);
    merged_recent_modifications.tick = 0;
    merged_recent_modifications.acc_predicted_chunk_mod_count = 0;
    merged_recent_modifications.acc_predicted_modifications = FL_MALLOC(chunk_modifications_t, MAX_ACCUMULATED_PREDICTED_CHUNK_MODIFICATIONS_PER_PACK);
}

static network_address_t bound_server_address = {};

static void s_process_connection_handshake(
    serialiser_t *serialiser,
    event_submissions_t *events) {
    packet_connection_handshake_t handshake = {};
    n_deserialise_connection_handshake(&handshake, serialiser);

    LOG_INFOV("Received handshake, there are %i players\n", handshake.player_count);

    event_enter_server_t *data = FL_MALLOC(event_enter_server_t, 1);
    data->info_count = handshake.player_count;
    data->infos = FL_MALLOC(player_init_info_t, data->info_count);

    int32_t highest_client_index = -1;
    
    // Add all the players
    for (uint32_t i = 0; i < handshake.player_count; ++i) {
        if (highest_client_index < (int32_t)handshake.player_infos[i].client_id) {
            highest_client_index = handshake.player_infos[i].client_id;
        }

        uint16_t client_id = handshake.player_infos[i].client_id;

        clients.data[client_id].client_id = client_id;
        clients.data[client_id].name = handshake.player_infos[i].name;
        clients.data[client_id].initialised = 1;

        data->infos[i].client_data = &clients.data[client_id];
        data->infos[i].ws_position = handshake.player_infos[i].ws_position;
        data->infos[i].ws_view_direction = handshake.player_infos[i].ws_view_direction;
        data->infos[i].ws_up_vector = handshake.player_infos[i].ws_up_vector;
        data->infos[i].default_speed = handshake.player_infos[i].default_speed;
        data->infos[i].is_local = handshake.player_infos[i].is_local;

        if (data->infos[i].is_local) {
            clients.data[client_id].chunks_to_wait_for = handshake.loaded_chunk_count;

            data->local_client_id = clients.data[i].client_id;
            current_client_id = data->local_client_id;
        }
    }

    submit_event(ET_ENTER_SERVER, data, events);
}

static void s_process_player_joined(
    serialiser_t *in_serialiser,
    event_submissions_t *events) {
    packet_player_joined_t packet = {};
    n_deserialise_player_joined(&packet, in_serialiser);

    LOG_INFOV("%s joined the game\n", packet.player_info.name);

    event_new_player_t *new_player = FL_MALLOC(event_new_player_t, 1);
    client_t *c = &clients.data[packet.player_info.client_id];
    c->name = packet.player_info.name;
    c->client_id = packet.player_info.client_id;

    new_player->info.client_data = c;
    new_player->info.ws_position = packet.player_info.ws_position;
    new_player->info.ws_view_direction = packet.player_info.ws_view_direction;
    new_player->info.ws_up_vector = packet.player_info.ws_up_vector;
    new_player->info.default_speed = packet.player_info.default_speed;
    new_player->info.is_local = 0;

    submit_event(ET_NEW_PLAYER, new_player, events);
}

static void s_process_player_left(
    serialiser_t *in_serialiser,
    event_submissions_t *events) {
    uint16_t disconnected_client = in_serialiser->deserialise_uint16();

    clients.data[disconnected_client].initialised = 0;

    event_player_disconnected_t *data = FL_MALLOC(event_player_disconnected_t, 1);
    data->client_id = disconnected_client;
    submit_event(ET_PLAYER_DISCONNECTED, data, events);

    LOG_INFO("Player disconnected\n");
}

#include <renderer/input.hpp>

// Debugging!
static bool simulate_lag = 0;

static void s_flag_modified_chunks(
    chunk_modifications_t *modifications,
    uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
        chunk_modifications_t *m_ptr = &modifications[i];
        chunk_t *c_ptr = get_chunk(ivector3_t(m_ptr->x, m_ptr->y, m_ptr->z));
        c_ptr->flags.modified_marker = 1;
        c_ptr->flags.index_of_modification_struct = i;
    }
}

static void s_unflag_modified_chunks(
    chunk_modifications_t *modifications,
    uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
        chunk_modifications_t *m_ptr = &modifications[i];
        chunk_t *c_ptr = get_chunk(ivector3_t(m_ptr->x, m_ptr->y, m_ptr->z));
        c_ptr->flags.modified_marker = 0;
        c_ptr->flags.index_of_modification_struct = 0;
    }
}

static void s_fill_dummy_voxels(
    chunk_modifications_t *modifications) {
    for (uint32_t i = 0; i < modifications->modified_voxels_count; ++i) {
        voxel_modification_t *v = &modifications->modifications[i];
        dummy_voxels[v->index] = i;
    }
}

static void s_unfill_dummy_voxels(
    chunk_modifications_t *modifications) {
    for (uint32_t i = 0; i < modifications->modified_voxels_count; ++i) {
        voxel_modification_t *v = &modifications->modifications[i];
        dummy_voxels[v->index] = SPECIAL_VALUE;
    }
}

static void s_merge_chunk_modifications(
    chunk_modifications_t *dst,
    uint32_t *dst_count,
    chunk_modifications_t *src,
    uint32_t src_count) {
    s_flag_modified_chunks(dst, *dst_count);

    for (uint32_t i = 0; i < src_count; ++i) {
        chunk_modifications_t *src_modifications = &src[i];

        chunk_t *chunk = get_chunk(ivector3_t(src_modifications->x, src_modifications->y, src_modifications->z));

        // Chunk has been terraformed on before (between previous game state dispatch and next one)
        if (chunk->flags.modified_marker) {
            // Index of modification struct would have been filled by s_flag_modiifed_chunks(), called above;
            chunk_modifications_t *dst_modifications = &dst[chunk->flags.index_of_modification_struct];

            // Has been modified, must fill dummy voxels
            s_fill_dummy_voxels(dst_modifications);

            for (uint32_t voxel = 0; voxel < src_modifications->modified_voxels_count; ++voxel) {
                voxel_modification_t *vm_ptr = &src_modifications->modifications[voxel];

                if (dummy_voxels[vm_ptr->index] == SPECIAL_VALUE) {
                    // Voxel has not yet been modified, can just push it into array
                    dst_modifications->modifications[dst_modifications->modified_voxels_count++] = *vm_ptr;
                }
                else {
                    // Voxel has been modified
                    uint32_t previous_index = dummy_voxels[vm_ptr->index];
                    // Just update final value
                    dst_modifications->modifications[previous_index].final_value = vm_ptr->final_value;
                }
            }

            s_unfill_dummy_voxels(dst_modifications);
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
    
    s_unflag_modified_chunks(dst, *dst_count);
}

static uint32_t s_fill_chunk_modification_array(
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

static accumulated_predicted_modification_t *s_accumulate_history() {
    accumulated_predicted_modification_t *next_acc = s_add_acc_predicted_modification();
    s_acc_predicted_modification_init(next_acc, get_current_tick());
    
    next_acc->acc_predicted_chunk_mod_count = s_fill_chunk_modification_array(next_acc->acc_predicted_modifications);

    if (next_acc->acc_predicted_chunk_mod_count == 0) {
        // Pops item that was just pushed
        acc_predicted_modifications.get_next_item_head();

        return NULL;
    }
    else {
        return next_acc;
    }
}

static void s_send_commands_to_server() {
    player_t *p = get_player(current_client_id);

    // Means that world hasn't been initialised yet (could be timing issues when submitting ENTER_SERVER
    // event and send commands interval, so just to make sure, check that player is not NULL)
    if (p) {
        if (simulate_lag) {
            p->cached_player_action_count = 0;
        }
        else {
            client_t *c = &clients[p->client_id];

            packet_player_commands_t packet = {};
            packet.did_correction = c->waiting_on_correction;
            packet.command_count = (uint8_t)p->cached_player_action_count;
            packet.actions = LN_MALLOC(player_actions_t, packet.command_count);

            for (uint32_t i = 0; i < packet.command_count; ++i) {
                packet.actions[i].bytes = p->cached_player_actions[i].bytes;
                packet.actions[i].dmouse_x = p->cached_player_actions[i].dmouse_x;
                packet.actions[i].dmouse_y = p->cached_player_actions[i].dmouse_y;
                packet.actions[i].dt = p->cached_player_actions[i].dt;
                packet.actions[i].accumulated_dt = p->cached_player_actions[i].accumulated_dt;
                packet.actions[i].tick = p->cached_player_actions[i].tick;
            }

            packet.ws_final_position = p->ws_position;
            packet.ws_final_view_direction = p->ws_view_direction;
            packet.ws_final_up_vector = p->ws_up_vector;

            accumulated_predicted_modification_t *next_acc = s_accumulate_history();
            if (next_acc) {
                // Packet will just take from the accumulation stuff
                packet.modified_chunk_count = next_acc->acc_predicted_chunk_mod_count;
                packet.chunk_modifications = next_acc->acc_predicted_modifications;
            }
            else {
                packet.modified_chunk_count = 0;
                packet.chunk_modifications = NULL;
            }

#if 0
            if (packet.modified_chunk_count) {
                printf("\n");
                LOG_INFOV("Modified %i chunks\n", packet.modified_chunk_count);
                for (uint32_t i = 0; i < packet.modified_chunk_count; ++i) {
                    for (uint32_t v = 0; v < packet.chunk_modifications[i].modified_voxels_count; ++v) {
                        LOG_INFOV("- index %i | final value %i\n", (int32_t)packet.chunk_modifications[i].modifications[v].index, (int32_t)packet.chunk_modifications[i].modifications[v].final_value);
                    }
                }
            }
#endif
            
            reset_modification_tracker();
            
            packet_header_t header = {};
            header.current_tick = get_current_tick();
            header.current_packet_count = current_packet;
            header.client_id = current_client_id;
            header.flags.packet_type = PT_CLIENT_COMMANDS;
            header.flags.total_packet_size = n_packed_packet_header_size() + n_packed_player_commands_size(&packet);

            serialiser_t serialiser = {};
            serialiser.init(header.flags.total_packet_size);
            n_serialise_packet_header(&header, &serialiser);
            n_serialise_player_commands(&packet, &serialiser);

            s_send_to(&serialiser, bound_server_address);
    
            p->cached_player_action_count = 0;

            c->waiting_on_correction = 0;

#if 0
            // For debugging purposes
            if (modified_chunk_count) {
                static uint32_t m_count = 0;
                ++m_count;
                if (m_count == 2) {
                    exit(1);
                }
            }
#endif
        }
    }
}

// Just for one accumulated_predicted_modification_t structure
static void s_revert_history_instance(
    accumulated_predicted_modification_t *apm_ptr) {
    for (uint32_t cm_index = 0; cm_index < apm_ptr->acc_predicted_chunk_mod_count; ++cm_index) {
        chunk_modifications_t *cm_ptr = &apm_ptr->acc_predicted_modifications[cm_index];
        chunk_t *c_ptr = get_chunk(ivector3_t(cm_ptr->x, cm_ptr->y, cm_ptr->z));

        for (uint32_t vm_index = 0; vm_index < cm_ptr->modified_voxels_count; ++vm_index) {
            voxel_modification_t *vm_ptr = &cm_ptr->modifications[vm_index];
            // Set all modified to initial values
            c_ptr->voxels[vm_ptr->index] = vm_ptr->initial_value;
        }

        c_ptr->flags.has_to_update_vertices = 1;
    }
}

static void s_revert_accumulated_modifications(
    uint64_t tick_until) {
    // First push all modifications that were done, so that we can revert most previous changes too
    s_accumulate_history();
    reset_modification_tracker();

    // Starts peeling off from the head
    accumulated_predicted_modification_t *current = acc_predicted_modifications.get_next_item_head();

    uint32_t total_available = acc_predicted_modifications.head_tail_difference;
    uint32_t removed_count = 1;

    uint64_t old_tick = current->tick;
    uint64_t new_tick = 0;
    
    while (current) {
        if (current->tick >= tick_until) {
            // Revert these changes
            s_revert_history_instance(current);

            ++removed_count;

            if (current->tick == tick_until) {
                // Simply break, don't revert anymore
                new_tick = tick_until;
                break;
            }
            else {
                // Peel off next modification
                current = acc_predicted_modifications.get_next_item_head();
            }
        }
        else {
            current = NULL;
            LOG_INFO("BULLLLLLLLLLLLLLSHIIIIIIIIIIIIIIITTTTTTTT ERRRRRRRRROOOOOOORRRRRRRR\n");
            break;
        }
    }

    LOG_INFOV("(Sent to revert to %llu) Reverted from %llu to %llu\n", (unsigned long long)tick_until, (unsigned long long)old_tick, (unsigned long long)new_tick);
}

static void s_correct_chunks(
    packet_game_state_snapshot_t *snapshot) {
    for (uint32_t cm_index = 0; cm_index < snapshot->modified_chunk_count; ++cm_index) {
        chunk_modifications_t *cm_ptr = &snapshot->chunk_modifications[cm_index];
        chunk_t *c_ptr = get_chunk(ivector3_t(cm_ptr->x, cm_ptr->y, cm_ptr->z));
        for (uint32_t vm_index = 0; vm_index < cm_ptr->modified_voxels_count; ++vm_index) {
            voxel_modification_t *vm_ptr = &cm_ptr->modifications[vm_index];
            //printf("(%i %i %i) Setting (%i) to %i\n", c_ptr->chunk_coord.x, c_ptr->chunk_coord.y, c_ptr->chunk_coord.z, vm_ptr->index, (int32_t)vm_ptr->final_value);
            c_ptr->voxels[vm_ptr->index] = vm_ptr->final_value;
        }
    }
}

static void s_process_game_state_snapshot(
    serialiser_t *serialiser,
    uint64_t received_tick,
    event_submissions_t *events) {
    packet_game_state_snapshot_t packet = {};
    n_deserialise_game_state_snapshot(&packet, serialiser);
    packet.chunk_modifications = n_deserialise_chunk_modifications(&packet.modified_chunk_count, serialiser);

    for (uint32_t i = 0; i < packet.player_data_count; ++i) {
        player_snapshot_t *snapshot = &packet.player_snapshots[i];

        if (snapshot->client_id == current_client_id) {
            client_t *c = &clients[snapshot->client_id];
            player_t *p = get_player(snapshot->client_id);

            // TODO: Watch out for this:
            if (snapshot->client_needs_to_correct_state && !snapshot->server_waiting_for_correction) {
                LOG_INFOV("Did correction at tick %llu!\n", (unsigned long long)snapshot->tick);

                // Do correction!
                p->ws_position = snapshot->ws_position;
                p->ws_view_direction = snapshot->ws_view_direction;
                p->ws_up_vector = snapshot->ws_up_vector;
                p->cached_player_action_count = 0;

                // Revert voxel modifications up from tick that server processed
                if (snapshot->terraformed) {
                    s_revert_accumulated_modifications(snapshot->terraform_tick);
                    s_correct_chunks(&packet);
                    // Sets all voxels to what the server has: client should be fully up to date, no need to interpolate between voxels
                }
                
                get_current_tick() = snapshot->tick;

                // Basically says that the client just did a correction - set correction flag on next packet sent to server
                c->waiting_on_correction = 1;
            }
            else {
#if 1
                if (snapshot->terraformed) {
                    LOG_INFOV("Syncing with tick: %llu\n", (unsigned long long)snapshot->terraform_tick);
                }

                // Mark all chunks / voxels that were modified from tick that server just processed, to current tick
                // These voxels should not be interpolated, and just left alone, because client just modified them
                // First make sure to finish interpolation of previous voxels
                chunks_to_interpolate_t *cti_ptr = get_chunks_to_interpolate();
                for (uint32_t cm_index = 0; cm_index < cti_ptr->modification_count; ++cm_index) {
                    chunk_modifications_t *cm_ptr = &cti_ptr->modifications[i];

                    chunk_t *c_ptr = get_chunk(ivector3_t(cm_ptr->x, cm_ptr->y, cm_ptr->z));

                    for (uint32_t vm_index = 0; vm_index < cm_ptr->modified_voxels_count; ++vm_index) {
                        voxel_modification_t *vm_ptr = &cm_ptr->modifications[vm_index];
                        c_ptr->voxels[vm_ptr->index] = vm_ptr->final_value;
                    }

                    cm_ptr->modified_voxels_count = 0;
                }

                cti_ptr->modification_count = 0;

                cti_ptr->elapsed = 0.0f;

                // Fill merged recent modifications
                s_acc_predicted_modification_init(&merged_recent_modifications, 0);

                uint32_t apm_index = acc_predicted_modifications.tail;
                for (uint32_t apm = 0; apm < acc_predicted_modifications.head_tail_difference; ++apm) {
                    accumulated_predicted_modification_t *apm_ptr = &acc_predicted_modifications.buffer[apm_index];
                    // For all modifications that were after the snapshot tick that server is sending us
                    if (apm_ptr->tick >= snapshot->tick) {
                        // Merge modifications
                        LOG_INFOV("Merging with tick %llu\n", apm_ptr->tick);
                        s_merge_chunk_modifications(
                            merged_recent_modifications.acc_predicted_modifications,
                            &merged_recent_modifications.acc_predicted_chunk_mod_count,
                            apm_ptr->acc_predicted_modifications,
                            apm_ptr->acc_predicted_chunk_mod_count);
                    }

                    apm_index = acc_predicted_modifications.increment_index(apm_index);
                }

                s_flag_modified_chunks(
                    merged_recent_modifications.acc_predicted_modifications,
                    merged_recent_modifications.acc_predicted_chunk_mod_count);

                for (uint32_t recv_cm_index = 0; recv_cm_index < packet.modified_chunk_count; ++recv_cm_index) {
                    chunk_modifications_t *recv_cm_ptr = &packet.chunk_modifications[recv_cm_index];
                    chunk_t *c_ptr = get_chunk(ivector3_t(recv_cm_ptr->x, recv_cm_ptr->y, recv_cm_ptr->z));

                    if (c_ptr->flags.modified_marker) {
                        chunk_modifications_t *dst_cm_ptr = &cti_ptr->modifications[cti_ptr->modification_count];

                        uint32_t local_cm_index = c_ptr->flags.index_of_modification_struct;
                        chunk_modifications_t *local_cm_ptr = &merged_recent_modifications.acc_predicted_modifications[local_cm_index];
                        // Chunk was flagged as modified, need to check voxel per voxel if we need to push this to chunks to interpolate
                        s_fill_dummy_voxels(local_cm_ptr);

                        uint32_t count = 0;
                        for (uint32_t recv_vm_index = 0; recv_vm_index < recv_cm_ptr->modified_voxels_count; ++recv_vm_index) {
                            voxel_modification_t *recv_vm_ptr = &recv_cm_ptr->modifications[recv_vm_index];
                            if (dummy_voxels[recv_vm_ptr->index] == SPECIAL_VALUE) {
                                if (recv_vm_ptr->final_value != c_ptr->voxels[recv_vm_ptr->index]) {
                                    // Was not modified, can push this
                                    dst_cm_ptr->modifications[dst_cm_ptr->modified_voxels_count].index = recv_vm_ptr->index;
                                    // Initial value is current value of voxel
                                    dst_cm_ptr->modifications[dst_cm_ptr->modified_voxels_count].initial_value = c_ptr->voxels[recv_vm_ptr->index];
                                    dst_cm_ptr->modifications[dst_cm_ptr->modified_voxels_count++].final_value = recv_vm_ptr->final_value;
                                    ++count;
                                }
                            }
                            else {
                                // Was modified, should not push
                                // TODO: Think about how to resolve these sort of conflicts
                            }
                        }

                        if (count) {
                            ++cti_ptr->modification_count;
                        }

                        s_unfill_dummy_voxels(local_cm_ptr);
                    }
                    else {
                        // Simple push this to chunks to interpolate
                        chunk_modifications_t *dst_cm_ptr = &cti_ptr->modifications[cti_ptr->modification_count];

                        if (recv_cm_ptr->modified_voxels_count) {
                            ++cti_ptr->modification_count;

                            dst_cm_ptr->x = recv_cm_ptr->x;
                            dst_cm_ptr->y = recv_cm_ptr->y;
                            dst_cm_ptr->z = recv_cm_ptr->z;
                            dst_cm_ptr->modified_voxels_count = recv_cm_ptr->modified_voxels_count;

                            //memcpy(dst_cm_ptr->modifications, recv_cm_ptr->modifications, sizeof(voxel_modification_t) * dst_cm_ptr->modified_voxels_count);
                            // Need to change initial value to current voxel values
                            for (uint32_t vm_index = 0; vm_index < dst_cm_ptr->modified_voxels_count; ++vm_index) {
                                voxel_modification_t *dst_vm_ptr = &dst_cm_ptr->modifications[vm_index];
                                voxel_modification_t *recv_vm_ptr = &recv_cm_ptr->modifications[vm_index];
                                dst_vm_ptr->index = recv_vm_ptr->index;
                                dst_vm_ptr->initial_value = c_ptr->voxels[recv_vm_ptr->index];
                                dst_vm_ptr->final_value = recv_vm_ptr->final_value;
                            }
                        }
                    }
                }

                if (cti_ptr->modification_count) {
                    LOG_INFO("Need to interpolate-----------------------------------------\n");
                }

                s_unflag_modified_chunks(
                    merged_recent_modifications.acc_predicted_modifications,
                    merged_recent_modifications.acc_predicted_chunk_mod_count);

#endif
                //cti_ptr->modification_count = 0;

                if (snapshot->terraformed) {
                    // Need to remove all modifications from tail to tick
                    accumulated_predicted_modification_t *current = acc_predicted_modifications.get_next_item_tail();
                    uint32_t count = 1;

                    // Pop all modifications until last tick that server processed
                    while (current != NULL) {
                        if (current->acc_predicted_chunk_mod_count) {
                            printf("\n");
                            LOG_INFOV("Cleared %i chunks\n", current->acc_predicted_chunk_mod_count);
                        }
                        
                        if (current->tick == snapshot->terraform_tick) {
                            LOG_INFOV("(To tick %llu) Cleared %i, there are %i left\n", (unsigned long long)snapshot->terraform_tick, (int32_t)count, acc_predicted_modifications.head_tail_difference);
                            break;
                        }
                        else if (current->tick > snapshot->terraform_tick) {
                            LOG_ERROR("ERRORRRORORORORRO\n");
                        }

                        current = acc_predicted_modifications.get_next_item_tail();
                        ++count;
                    }

                    if (current == NULL && count > 1) {
                        LOG_INFO("Didn't clear any\n");
                    }
                }
            }
        }
        else {
            player_t *p = get_player(snapshot->client_id);
            
            p->remote_snapshots.push_item(snapshot);
        }
    }
}

static void s_process_chunk_voxels(
    serialiser_t *serialiser,
    event_submissions_t *events) {
    uint32_t loaded_chunk_count = serialiser->deserialise_uint32();

    for (uint32_t c = 0; c < loaded_chunk_count; ++c) {
        int16_t x = serialiser->deserialise_int16();
        int16_t y = serialiser->deserialise_int16();
        int16_t z = serialiser->deserialise_int16();

        chunk_t *chunk = get_chunk(ivector3_t(x, y, z));
        chunk->flags.has_to_update_vertices = 1;

        for (uint32_t v = 0; v < CHUNK_EDGE_LENGTH * CHUNK_EDGE_LENGTH * CHUNK_EDGE_LENGTH;) {
            uint8_t current_value = serialiser->deserialise_uint8();

            if (current_value == SPECIAL_VALUE) {
                chunk->voxels[v] = 0;
                ++v;

                // Repeating zeros
                uint32_t zero_count = serialiser->deserialise_uint32();
                chunk->voxels[v + 1] = 0;
                chunk->voxels[v + 2] = 0;
                chunk->voxels[v + 3] = 0;
                chunk->voxels[v + 4] = 0;

                v += 4;

                uint32_t previous_v = v;
                for (; v < previous_v + zero_count - 5; ++v) {
                    chunk->voxels[v] = 0;
                }
            }
            else {
                chunk->voxels[v] = current_value;
                ++v;
            }
        }
    }

    LOG_INFOV("Received %i chunks\n", loaded_chunk_count);
}

void tick_client(
    event_submissions_t *events) {
    raw_input_t *input = get_raw_input();
    if (input->buttons[BT_F].instant) {
        simulate_lag = !simulate_lag;

        if (simulate_lag) {LOG_INFO("Simulating lag\n");}
        else {LOG_INFO("Not simulating lag\n");}
    }
    
    if (client_check_incoming_packets) {
        static float elapsed = 0.0f;
        elapsed += logic_delta_time();
        if (elapsed >= client_command_output_interval) {
            // Send commands to the server
            s_send_commands_to_server();

            elapsed = 0.0f;
        }

        network_address_t received_address = {};
        int32_t received = receive_from(
            main_udp_socket,
            message_buffer,
            sizeof(char) * MAX_MESSAGE_SIZE,
            &received_address);

        // In future, separate thread will be capturing all these packets
        static const uint32_t MAX_RECEIVED_PER_TICK = 4;
        uint32_t i = 0;

        while (received) {
            serialiser_t in_serialiser = {};
            in_serialiser.data_buffer = (uint8_t *)message_buffer;
            in_serialiser.data_buffer_size = received;

            packet_header_t header = {};
            n_deserialise_packet_header(&header, &in_serialiser);

            switch(header.flags.packet_type) {

            case PT_CONNECTION_HANDSHAKE: {
                s_process_connection_handshake(
                    &in_serialiser,
                    events);
                return;
            } break;

            case PT_PLAYER_JOINED: {
                s_process_player_joined(
                    &in_serialiser,
                    events);
            } break;

            case PT_PLAYER_LEFT: {
                s_process_player_left(
                    &in_serialiser,
                    events);
            } break;

            case PT_GAME_STATE_SNAPSHOT: {
                s_process_game_state_snapshot(
                    &in_serialiser,
                    header.current_tick,
                    events);
            } break;

            case PT_CHUNK_VOXELS: {
                s_process_chunk_voxels(
                    &in_serialiser,
                    events);
            } break;

            default: {
                LOG_INFO("Received unidentifiable packet\n");
            } break;

            }

            if (i < MAX_RECEIVED_PER_TICK) {
                received = receive_from(
                    main_udp_socket,
                    message_buffer,
                    sizeof(char) * MAX_MESSAGE_SIZE,
                    &received_address);
            }
            else {
                received = false;
            } 

            ++i;
        }
    }
}

static void s_send_connect_request_to_server(
    const char *ip_address,
    local_client_info_t *info) {
    bound_server_address.port = host_to_network_byte_order(GAME_OUTPUT_PORT_SERVER);
    bound_server_address.ipv4_address = str_to_ipv4_int32(ip_address, GAME_OUTPUT_PORT_SERVER, SP_UDP);

    serialiser_t serialiser = {};
    serialiser.init(100);

    packet_connection_request_t request = {};
    request.name = info->name;
    
    packet_header_t header = {};
    header.flags.packet_type = PT_CONNECTION_REQUEST;
    header.flags.total_packet_size = n_packed_packet_header_size() + n_packed_connection_request_size(&request);
    header.current_packet_count = current_packet;

    n_serialise_packet_header(&header, &serialiser);
    n_serialise_connection_request(&request, &serialiser);

    if (s_send_to(&serialiser, bound_server_address)) {
        LOG_INFO("Success sent connection request\n");
        client_check_incoming_packets = 1;
    }
    else {
        LOG_ERROR("Failed to send connection request\n");
    }
}

static void s_send_disconnect_to_server() {
    serialiser_t serialiser = {};
    serialiser.init(100);

    packet_header_t header = {};
    header.current_tick = get_current_tick();
    header.current_packet_count = current_packet;
    header.client_id = current_client_id;
    header.flags.packet_type = PT_CLIENT_DISCONNECT;
    header.flags.total_packet_size = n_packed_packet_header_size();

    n_serialise_packet_header(&header, &serialiser);
    
    s_send_to(&serialiser, bound_server_address);
}

static bool started_server = 0;

static void s_start_server() {
    memset(dummy_voxels, SPECIAL_VALUE, sizeof(dummy_voxels));

    s_main_udp_socket_init(GAME_OUTPUT_PORT_SERVER);

    clients.init(MAX_CLIENTS);

    started_server = 1;

    set_chunk_history_tracker_value(1);

    acc_predicted_modifications.init();

    uint32_t sizeof_chunk_mod_pack = sizeof(chunk_modifications_t) * MAX_PREDICTED_CHUNK_MODIFICATIONS;

    chunk_modification_allocator.pool_init(
        sizeof_chunk_mod_pack,
        sizeof_chunk_mod_pack * MAX_ACCUMULATED_PREDICTED_CHUNK_MODIFICATIONS_PER_PACK + 4);
}

static bool s_send_handshake(
    uint16_t client_id,
    event_new_player_t *player_info,
    uint32_t loaded_chunk_count) {
    packet_connection_handshake_t connection_handshake = {};
    connection_handshake.loaded_chunk_count = loaded_chunk_count;
    connection_handshake.player_infos = LN_MALLOC(full_player_info_t, clients.data_count);

    for (uint32_t i = 0; i < clients.data_count; ++i) {
        client_t *client = clients.get(i);
        if (client->initialised) {
            full_player_info_t *info = &connection_handshake.player_infos[connection_handshake.player_count];

            if (i == client_id) {
                info->name = client->name;
                info->client_id = client->client_id;
                info->ws_position = player_info->info.ws_position;
                info->ws_view_direction = player_info->info.ws_view_direction;
                info->ws_up_vector = player_info->info.ws_up_vector;
                info->default_speed = player_info->info.default_speed;
                info->is_local = 1;
            }
            else {
                player_t *p = get_player(i);
                info->name = client->name;
                info->client_id = client->client_id;
                info->ws_position = p->ws_position;
                info->ws_view_direction = p->ws_view_direction;
                info->ws_up_vector = p->ws_up_vector;
                info->default_speed = p->default_speed;
                info->is_local = 0;
            }
            
            ++connection_handshake.player_count;
        }
    }

    packet_header_t header = {};
    header.current_tick = get_current_tick();
    header.flags.total_packet_size = n_packed_packet_header_size() + n_packed_connection_handshake_size(&connection_handshake);
    header.flags.packet_type = PT_CONNECTION_HANDSHAKE;

    serialiser_t serialiser = {};
    serialiser.init(header.flags.total_packet_size);

    n_serialise_packet_header(
        &header,
        &serialiser);
    
    n_serialise_connection_handshake(
        &connection_handshake,
        &serialiser);

    client_t *c = clients.get(client_id);

    if (s_send_to(&serialiser, c->address)) {
        LOG_INFOV("Sent handshake to client: %s\n", c->name);
        return 1;
    }
    else {
        LOG_INFOV("Failed to send handshake to client: %s\n", c->name);
        return 0;
    }
}

static constexpr uint32_t maximum_chunks_per_packet() {
    return ((65507 - sizeof(uint32_t)) / (sizeof(int16_t) * 3 + CHUNK_EDGE_LENGTH * CHUNK_EDGE_LENGTH * CHUNK_EDGE_LENGTH));
}

static void s_serialise_chunk(
    serialiser_t *serialiser,
    uint32_t *chunks_in_packet,
    voxel_chunk_values_t *values,
    uint32_t i) {
    voxel_chunk_values_t *current_values = &values[i];

    serialiser->serialise_int16(current_values->x);
    serialiser->serialise_int16(current_values->y);
    serialiser->serialise_int16(current_values->z);

    for (uint32_t v_index = 0; v_index < CHUNK_EDGE_LENGTH * CHUNK_EDGE_LENGTH * CHUNK_EDGE_LENGTH; ++v_index) {
        uint8_t current_voxel = current_values->voxel_values[v_index];
        if (current_voxel == 0) {
            uint32_t before_head = serialiser->data_buffer_head;

            uint32_t zero_count = 0;
            for (; current_values->voxel_values[v_index] == 0 && zero_count < 5; ++v_index, ++zero_count) {
                serialiser->serialise_uint8(0);
            }
            

            if (zero_count == 5) {
                for (; current_values->voxel_values[v_index] == 0; ++v_index, ++zero_count) {}

                serialiser->data_buffer_head = before_head;
                serialiser->serialise_uint8(SPECIAL_VALUE);
                serialiser->serialise_uint32(zero_count);
            }

            v_index -= 1;
        }
        else {
            serialiser->serialise_uint8(current_voxel);
        }
    }

    *chunks_in_packet = *chunks_in_packet + 1;
}

static void s_serialise_voxel_chunks_and_send(
    client_t *client,
    voxel_chunk_values_t *values,
    uint32_t count) {
    packet_header_t header = {};
    header.flags.packet_type = PT_CHUNK_VOXELS;
    header.flags.total_packet_size = 15 * (3 * sizeof(int16_t) + CHUNK_EDGE_LENGTH * CHUNK_EDGE_LENGTH * CHUNK_EDGE_LENGTH);
    header.current_tick = get_current_tick();
    header.current_packet_count = current_packet;
    
    serialiser_t serialiser = {};
    serialiser.init(header.flags.total_packet_size);

    n_serialise_packet_header(&header, &serialiser);
    
    uint32_t chunks_in_packet = 0;

    uint8_t *chunk_count_byte = &serialiser.data_buffer[serialiser.data_buffer_head];

    // For now serialise 0
    serialiser.serialise_uint32(0);

    uint32_t chunk_values_start = serialiser.data_buffer_head;
    
    for (uint32_t i = 0; i < count; ++i) {
        s_serialise_chunk(&serialiser, &chunks_in_packet, values, i);

        if (serialiser.data_buffer_head + 3 * sizeof(int16_t) + CHUNK_EDGE_LENGTH * CHUNK_EDGE_LENGTH * CHUNK_EDGE_LENGTH > serialiser.data_buffer_size ||
            i + 1 == count) {
            // Need to send in new packet
            serialiser.serialise_uint32(chunks_in_packet, chunk_count_byte);

            if (s_send_to(&serialiser, client->address)) {
                LOG_INFOV("Sent %i chunks to %s\n", chunks_in_packet, client->name);
            }
            else {
                LOG_INFOV("Failed to send %i chunks to %s\n", chunks_in_packet, client->name);
            }

            serialiser.data_buffer_head = chunk_values_start;
            chunks_in_packet = 0;
        }
    }
}

static void s_send_game_state_to_new_client(
    uint16_t client_id,
    event_new_player_t *player_info) {
    uint32_t loaded_chunk_count = 0;
    chunk_t **chunks = get_active_chunks(&loaded_chunk_count);

    if (s_send_handshake(
        client_id,
        player_info,
        loaded_chunk_count)) {
        client_t *client = clients.get(client_id);

        // Send chunk information
        uint32_t max_chunks_per_packet = maximum_chunks_per_packet();
        LOG_INFOV("Maximum chunks per packet: %i\n", max_chunks_per_packet);

        voxel_chunk_values_t *voxel_chunks = LN_MALLOC(voxel_chunk_values_t, loaded_chunk_count);

        uint32_t count = 0;
        for (uint32_t i = 0; i < loaded_chunk_count; ++i) {
            chunk_t *c = chunks[i];
            if (c) {
                voxel_chunks[count].x = c->chunk_coord.x;
                voxel_chunks[count].y = c->chunk_coord.y;
                voxel_chunks[count].z = c->chunk_coord.z;

                voxel_chunks[count].voxel_values = c->voxels;

                ++count;
            }
        }

        loaded_chunk_count = count;
        
        s_serialise_voxel_chunks_and_send(client, voxel_chunks, loaded_chunk_count);
    }
}

static void s_inform_all_players_on_newcomer(
    event_new_player_t *info) {
    packet_player_joined_t packet = {};
    packet.player_info.name = info->info.client_data->name;
    packet.player_info.client_id = info->info.client_data->client_id;
    packet.player_info.ws_position = info->info.ws_position;
    packet.player_info.ws_view_direction = info->info.ws_view_direction;
    packet.player_info.ws_up_vector = info->info.ws_up_vector;
    packet.player_info.default_speed = info->info.default_speed;
    packet.player_info.is_local = 0;

    packet_header_t header = {};
    header.current_tick = get_current_tick();
    header.current_packet_count = current_packet;
    header.flags.total_packet_size = n_packed_packet_header_size() + n_packed_player_joined_size(&packet);
    header.flags.packet_type = PT_PLAYER_JOINED;
    
    serialiser_t serialiser = {};
    serialiser.init(header.flags.total_packet_size);
    
    n_serialise_packet_header(&header, &serialiser);
    n_serialise_player_joined(&packet, &serialiser);
    
    for (uint32_t i = 0; i < clients.data_count; ++i) {
        if (i != packet.player_info.client_id) {
            client_t *c = clients.get(i);
            if (c->initialised) {
                s_send_to(&serialiser, c->address);
            }
        }
    }
}

static void s_process_connection_request(
    serialiser_t *serialiser,
    network_address_t address,
    event_submissions_t *events) {
    packet_connection_request_t request = {};
    n_deserialise_connection_request(&request, serialiser);

    uint32_t client_id = clients.add();
    client_t *client = clients.get(client_id);
    
    client->initialised = 1;
    client->client_id = client_id;
    client->name = create_fl_string(request.name);
    client->address = address;
    client->received_first_commands_packet = 0;
    client->predicted_chunk_mod_count = 0;
    client->predicted_modifications = (chunk_modifications_t *)chunk_modification_allocator.allocate_arena();
    memset(client->predicted_modifications, 0, sizeof(chunk_modifications_t) * MAX_PREDICTED_CHUNK_MODIFICATIONS);
    
    event_new_player_t *event_data = FL_MALLOC(event_new_player_t, 1);
    event_data->info.client_data = client;
    // Need to calculate a random position
    // TODO: In future, make it so that there is like some sort of startup screen when joining a server (like choose team, etc..)
    event_data->info.ws_position = vector3_t(0.0f);
    event_data->info.ws_view_direction = vector3_t(1.0f, 0.0f, 0.0f);
    event_data->info.ws_up_vector = vector3_t(0.0f, 1.0f, 0.0f);
    event_data->info.default_speed = 10.0f;
    event_data->info.is_local = 0;
    
    submit_event(ET_NEW_PLAYER, event_data, events);

    // Send game state to new player
    s_send_game_state_to_new_client(client_id, event_data);
    // Dispatch to all players newly joined player information
    s_inform_all_players_on_newcomer(event_data);
}

static void s_process_client_disconnect(
    serialiser_t *serialiser,
    uint16_t client_id,
    event_submissions_t *events) {
    (void)serialiser;
    LOG_INFO("Client disconnected\n");

    clients[client_id].initialised = 0;
    clients.remove(client_id);

    event_player_disconnected_t *data = FL_MALLOC(event_player_disconnected_t, 1);
    data->client_id = client_id;
    submit_event(ET_PLAYER_DISCONNECTED, data, events);

    serialiser_t out_serialiser = {};
    out_serialiser.init(100);
    packet_header_t header = {};
    header.current_tick = get_current_tick();
    header.current_packet_count = current_packet;
    header.flags.packet_type = PT_PLAYER_LEFT;
    header.flags.total_packet_size = n_packed_packet_header_size() + sizeof(uint16_t);

    n_serialise_packet_header(&header, &out_serialiser);
    out_serialiser.serialise_uint16(client_id);
    
    for (uint32_t i = 0; i < clients.data_count; ++i) {
        client_t *c = &clients[i];
        if (c->initialised) {
            s_send_to(&out_serialiser, c->address);
        }
    }
}

static void s_handle_chunk_modifications(
    packet_player_commands_t *commands,
    client_t *client) {
    s_merge_chunk_modifications(
        client->predicted_modifications,
        &client->predicted_chunk_mod_count,
        commands->chunk_modifications,
        commands->modified_chunk_count);
}

static void s_process_client_commands(
    serialiser_t *serialiser,
    uint16_t client_id,
    uint64_t tick,
    event_submissions_t *events) {
    (void)events;

    player_t *p = get_player(client_id);

    if (p) {
        client_t *c = &clients[p->client_id];

        c->received_first_commands_packet = 1;

        packet_player_commands_t commands = {};
        n_deserialise_player_commands(&commands, serialiser);

        if (commands.did_correction) {
            LOG_INFOV("Did correction: %s\n", glm::to_string(p->ws_position).c_str());
            c->waiting_on_correction = 0;
        }

        // Only process client commands if we are not waiting on a correction
        if (!c->waiting_on_correction) {
            // Tick at which client sent packet (since last game state snapshot dispatch)
            if (c->should_set_tick) {
                c->tick = tick;
                c->should_set_tick = 0;
            }

            for (uint32_t i = 0; i < commands.command_count; ++i) {
                push_player_actions(p, &commands.actions[i]);
            }

            c->ws_predicted_position = commands.ws_final_position;
            c->ws_predicted_view_direction = commands.ws_final_view_direction;
            c->ws_predicted_up_vector = commands.ws_final_up_vector;

            // Process terraforming stuff
            if (commands.modified_chunk_count) {
                LOG_INFOV("(Tick %llu) Received %i chunk modifications\n", (unsigned long long)c->tick, commands.modified_chunk_count);
                c->did_terrain_mod_previous_tick = 1;
                c->tick_at_which_client_terraformed = tick;
                s_handle_chunk_modifications(&commands, c);
            }
        }
    }
    else {
        // There is a problem
        LOG_ERROR("Player was not initialised yet, cannot process client commands!\n");
    }
}

static bool s_check_if_client_has_to_correct_state(
    player_t *p,
    client_t *c) {
    vector3_t dposition = glm::abs(p->ws_position - c->ws_predicted_position);
    vector3_t ddirection = glm::abs(p->ws_view_direction - c->ws_predicted_view_direction);
    vector3_t dup = glm::abs(p->ws_up_vector - c->ws_predicted_up_vector);

    float precision = 0.000001f;
    bool incorrect_position = 0;
    if (dposition.x >= precision || dposition.y >= precision || dposition.z >= precision) {
        incorrect_position = 1;

        LOG_INFOV("Position is wrong: %s => %s\n", glm::to_string(c->ws_predicted_position).c_str(), glm::to_string(p->ws_position).c_str());
    }

    bool incorrect_direction = 0;
    if (ddirection.x >= precision || ddirection.y >= precision || ddirection.z >= precision) {
        incorrect_direction = 1;

        LOG_INFOV("Direction is wrong: %s => %s\n", glm::to_string(c->ws_predicted_view_direction).c_str(), glm::to_string(p->ws_view_direction).c_str());
    }

    bool incorrect_up = 0;
    if (dup.x >= precision || dup.y >= precision || dup.z >= precision) {
        incorrect_up = 1;

        LOG_INFOV("Up is wrong: %s => %s\n", glm::to_string(c->ws_predicted_up_vector).c_str(), glm::to_string(p->ws_up_vector).c_str());
    }

    return incorrect_position || incorrect_direction || incorrect_up;
}

static bool s_check_if_client_has_to_correct_terrain(
    client_t *c) {
    bool needs_to_correct = 0;

    for (uint32_t cm_index = 0; cm_index < c->predicted_chunk_mod_count; ++cm_index) {
        chunk_modifications_t *cm_ptr = &c->predicted_modifications[cm_index];
        cm_ptr->needs_to_correct = 0;

        chunk_t *c_ptr = get_chunk(ivector3_t(cm_ptr->x, cm_ptr->y, cm_ptr->z));

        bool chunk_has_mistake = 0;
        
        for (uint32_t vm_index = 0; vm_index < cm_ptr->modified_voxels_count; ++vm_index) {
            voxel_modification_t *vm_ptr = &cm_ptr->modifications[vm_index];

            uint8_t actual_value = c_ptr->voxels[vm_ptr->index];
            uint8_t predicted_value = vm_ptr->final_value;

            // Just one mistake can completely mess stuff up between the client and server
            if (actual_value != predicted_value) {
                //printf("(%i %i %i) Need to set (%i) %i -> %i\n", c_ptr->chunk_coord.x, c_ptr->chunk_coord.y, c_ptr->chunk_coord.z, vm_ptr->index, (int32_t)predicted_value, (int32_t)actual_value);

                chunk_has_mistake = 1;
            }
        }

        if (chunk_has_mistake) {
            LOG_INFOV("(Tick %llu)Above mistakes were in chunk (%i %i %i)\n", (unsigned long long)c->tick, c_ptr->chunk_coord.x, c_ptr->chunk_coord.y, c_ptr->chunk_coord.z);

            needs_to_correct = 1;
            cm_ptr->needs_to_correct = 1;
        }
    }

    return needs_to_correct;
}

// Send the voxel modifications
// It will the client's job to decide which voxels to interpolate between based on the
// fact that it knows which voxels it modified - it will not interpolate between voxels it knows to have modified
// in the time frame that concerns this state dispatch
static void s_add_chunk_modifications_to_game_state_snapshot(
    packet_game_state_snapshot_t *snapshot) {
    // Up to 300 chunks can be modified between game dispatches
    chunk_modifications_t *modifications = LN_MALLOC(chunk_modifications_t, MAX_ACCUMULATED_PREDICTED_CHUNK_MODIFICATIONS_PER_PACK);

    uint32_t modification_count = s_fill_chunk_modification_array(modifications);

    snapshot->modified_chunk_count = modification_count;
    snapshot->chunk_modifications = modifications;
}

static void s_dispatch_game_state_snapshot() {
#if 0
    putchar('\n');
    LOG_INFO("--------------------- DISPATCH ---------------------\n");
#endif
    
    packet_game_state_snapshot_t packet = {};
    packet.player_data_count = 0;
    packet.player_snapshots = LN_MALLOC(player_snapshot_t, clients.data_count);

    s_add_chunk_modifications_to_game_state_snapshot(&packet);
    
    for (uint32_t i = 0; i < clients.data_count; ++i) {
        client_t *c = &clients[i];

        if (c->initialised && c->received_first_commands_packet) {
            c->should_set_tick = 1;

            // Check if the data that the client predicted was correct, if not, force client to correct position
            // Until server is sure that the client has done a correction, server will not process this client's commands
            player_snapshot_t *snapshot = &packet.player_snapshots[packet.player_data_count];
            snapshot->flags = 0;

            player_t *p = get_player(c->client_id);
            // Check if 
            bool has_to_correct_state = s_check_if_client_has_to_correct_state(p, c);
            // Check if client has to correct voxel modifications
            bool has_to_correct_terrain = s_check_if_client_has_to_correct_terrain(c);

            if (has_to_correct_state || has_to_correct_terrain) {
                if (c->waiting_on_correction) {
                    LOG_INFO("Client needs to do correction, but did not receive correction acknowledgement, not sending correction\n");
                    snapshot->client_needs_to_correct_state = 0;
                    snapshot->server_waiting_for_correction = 1;
                }
                else {
                    // If there is a correction of any kind to do, force client to correct everything
                    c->waiting_on_correction = 1;

                    LOG_INFOV("Client needs to revert to tick %llu\n", (unsigned long long)c->tick_at_which_client_terraformed);
                    snapshot->client_needs_to_correct_state = has_to_correct_state || has_to_correct_terrain;
                    snapshot->server_waiting_for_correction = 0;
                }
            }

            snapshot->client_id = c->client_id;
            snapshot->ws_position = p->ws_position;
            snapshot->ws_view_direction = p->ws_view_direction;
            snapshot->ws_up_vector = p->ws_up_vector;
            snapshot->tick = c->tick;
            snapshot->terraformed = c->did_terrain_mod_previous_tick;
            
            if (snapshot->terraformed) {
                snapshot->terraform_tick = c->tick_at_which_client_terraformed;
            }

            // Reset
            c->did_terrain_mod_previous_tick = 0;
            c->tick_at_which_client_terraformed = 0;

            ++packet.player_data_count;
        }
    }

    packet_header_t header = {};
    header.current_tick = get_current_tick();
    header.current_packet_count = current_packet;
    // Don't need to fill this
    header.client_id = 0;
    header.flags.packet_type = PT_GAME_STATE_SNAPSHOT;
    header.flags.total_packet_size = n_packed_packet_header_size() + n_packed_game_state_snapshot_size(&packet);

    serialiser_t serialiser = {};
    serialiser.init(header.flags.total_packet_size);

    n_serialise_packet_header(&header, &serialiser);

    // This is the packet for players that need correction
    n_serialise_game_state_snapshot(&packet, &serialiser);
    n_serialise_chunk_modifications(packet.chunk_modifications, packet.modified_chunk_count, &serialiser);

    for (uint32_t i = 0; i < clients.data_count; ++i) {
        client_t *c = &clients[i];

        if (c->initialised && c->received_first_commands_packet) {
            s_send_to(&serialiser, c->address);
        }
        
        // Clear client's predicted modification array
        c->predicted_chunk_mod_count = 0;
    }

    reset_modification_tracker();
}

void tick_server(
    event_submissions_t *events) {
    // In future, have a separate thread do this kind of stuff
    static float elapsed = 0.0f;
    elapsed += logic_delta_time();
    if (elapsed >= server_snapshot_output_interval) {
        // Send commands to the server
        s_dispatch_game_state_snapshot();

        elapsed = 0.0f;
    }

    for (uint32_t i = 0; i < clients.data_count + 1; ++i) {
        network_address_t received_address = {};
        int32_t received = receive_from(
            main_udp_socket,
            message_buffer,
            sizeof(char) * MAX_MESSAGE_SIZE,
            &received_address);

        if (received) {
            serialiser_t in_serialiser = {};
            in_serialiser.data_buffer = (uint8_t *)message_buffer;
            in_serialiser.data_buffer_size = received;

            packet_header_t header = {};
            n_deserialise_packet_header(&header, &in_serialiser);

            switch(header.flags.packet_type) {

            case PT_CONNECTION_REQUEST: {
                s_process_connection_request(
                    &in_serialiser,
                    received_address,
                    events);
            } break;

            case PT_CLIENT_DISCONNECT: {
                s_process_client_disconnect(
                    &in_serialiser,
                    header.client_id,
                    events);
            } break;

            case PT_CLIENT_COMMANDS: {
                s_process_client_commands(
                    &in_serialiser,
                    header.client_id,
                    header.current_tick,
                    events);
            } break;

            }
        }
    }
}

void tick_net(
    event_submissions_t *events) {
    if (started_client) {
        tick_client(events);
    }
    if (started_server) {
        tick_server(events);
    }
}

static listener_t net_listener_id;

static void s_net_event_listener(
    void *object,
    event_t *event) {
    switch (event->type) {

    case ET_START_CLIENT: {
        s_start_client();
    } break;

    case ET_START_SERVER: {
        s_start_server();
    } break;

    case ET_REQUEST_TO_JOIN_SERVER: {
        event_data_request_to_join_server_t *data = (event_data_request_to_join_server_t *)event->data;
        local_client_info_t client_info;
        client_info.name = data->client_name;
        s_send_connect_request_to_server(data->ip_address, &client_info);

        FL_FREE(data);
    } break;

    case ET_LEAVE_SERVER: {
        if (bound_server_address.ipv4_address > 0) {
            // Send to server message
            s_send_disconnect_to_server();
        
            memset(&bound_server_address, 0, sizeof(bound_server_address));
            memset(clients.data, 0, sizeof(client_t) * clients.max_size);

            client_check_incoming_packets = 0;
        
            LOG_INFO("Disconnecting\n");
        }
    } break;

    }
}

void net_init(
    event_submissions_t *events) {
    net_listener_id = set_listener_callback(
        &s_net_event_listener,
        NULL,
        events);

    subscribe_to_event(ET_START_CLIENT, net_listener_id, events);
    subscribe_to_event(ET_START_SERVER, net_listener_id, events);
    subscribe_to_event(ET_REQUEST_TO_JOIN_SERVER, net_listener_id, events);
    subscribe_to_event(ET_LEAVE_SERVER, net_listener_id, events);

    socket_api_init();

    message_buffer = FL_MALLOC(char, MAX_MESSAGE_SIZE);
}

bool connected_to_server() {
    return bound_server_address.ipv4_address != 0;
}

float server_snapshot_interval() {
    return server_snapshot_output_interval;
}

float client_command_interval() {
    return client_command_output_interval;
}
