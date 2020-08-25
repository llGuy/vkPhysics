#include "cl_main.hpp"
#include "nw_client.hpp"
#include "wd_interp.hpp"
#include "wd_predict.hpp"
#include <common/net.hpp>
#include <common/game.hpp>
#include <common/event.hpp>
#include <common/socket.hpp>
#include <common/string.hpp>
#include "nw_client_meta.hpp"
#include <common/constant.hpp>
#include <common/meta_packet.hpp>
#include <common/game_packet.hpp>
#include <cstddef>

struct client_info_t {
    const char *client_name;
};

static client_info_t local_client_info;

static float client_command_output_interval = 1.0f / 25.0f;

static bool started_client = 0;
static uint16_t current_client_id;

uint16_t nw_get_local_client_index() {
    return current_client_id;
}

static bool client_check_incoming_packets = 0;

static bool still_receiving_chunk_packets;
static uint32_t chunks_to_receive;

static void s_start_client(
    event_start_client_t *data) {
    still_receiving_chunk_packets = 0;
    chunks_to_receive = 0;

    memset(g_net_data.dummy_voxels, CHUNK_SPECIAL_VALUE, sizeof(g_net_data.dummy_voxels));

    main_udp_socket_init(GAME_OUTPUT_PORT_CLIENT);

    g_net_data.clients.init(NET_MAX_CLIENT_COUNT);
    
    started_client = 1;

    track_modification_history();

    g_net_data.acc_predicted_modifications.init();

    uint32_t sizeof_chunk_mod_pack = sizeof(chunk_modifications_t) * MAX_PREDICTED_CHUNK_MODIFICATIONS;

    g_net_data.chunk_modification_allocator.pool_init(
        sizeof_chunk_mod_pack,
        sizeof_chunk_mod_pack * (NET_MAX_ACCUMULATED_PREDICTED_CHUNK_MODIFICATIONS_PER_PACK + 4));

    //s_acc_predicted_modification_init(&merged_recent_modifications, 0);
    g_net_data.merged_recent_modifications.tick = 0;
    g_net_data.merged_recent_modifications.acc_predicted_chunk_mod_count = 0;
    g_net_data.merged_recent_modifications.acc_predicted_modifications = FL_MALLOC(chunk_modifications_t, NET_MAX_ACCUMULATED_PREDICTED_CHUNK_MODIFICATIONS_PER_PACK);

    // Send to meta server information about client
    meta_packet_header_t header = {};
    header.type = HPT_QUERY_CLIENT_REGISTER;

    meta_query_client_register_t register_packet = {};
    local_client_info.client_name = data->client_name;
    register_packet.client_name = local_client_info.client_name;

    serialiser_t serialiser = {};
    serialiser.init(100);

    serialise_meta_packet_header(&header, &serialiser);
    serialise_meta_query_client_register(&register_packet, &serialiser);

    send_to_meta_server(&serialiser);
}

static network_address_t bound_server_address = {};

// PT_CONNECTION_HANDSHAKE
static void s_receive_packet_connection_handshake(
    serialiser_t *serialiser,
    event_submissions_t *events) {
    packet_connection_handshake_t handshake = {};
    deserialise_connection_handshake(&handshake, serialiser);

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

        g_net_data.clients.data[client_id].client_id = client_id;
        g_net_data.clients.data[client_id].name = handshake.player_infos[i].name;
        g_net_data.clients.data[client_id].initialised = 1;

        data->infos[i].client_name = g_net_data.clients.data[client_id].name;
        data->infos[i].client_id = client_id;
        data->infos[i].ws_position = handshake.player_infos[i].ws_position;
        data->infos[i].ws_view_direction = handshake.player_infos[i].ws_view_direction;
        data->infos[i].ws_up_vector = handshake.player_infos[i].ws_up_vector;
        data->infos[i].default_speed = handshake.player_infos[i].default_speed;
        data->infos[i].flags = handshake.player_infos[i].flags.u32;

        if (handshake.player_infos[i].flags.is_local) {
            LOG_INFOV("Current client ID is %i\n", g_net_data.clients.data[i].client_id);

            g_net_data.clients.data[client_id].chunks_to_wait_for = handshake.loaded_chunk_count;

            data->infos[i].next_random_spawn_position = handshake.player_infos[i].ws_next_random_position;

            data->local_client_id = g_net_data.clients.data[i].client_id;
            current_client_id = data->local_client_id;
        }
    }

    submit_event(ET_ENTER_SERVER, data, events);

    if (handshake.loaded_chunk_count) {
        still_receiving_chunk_packets = 1;
        chunks_to_receive = handshake.loaded_chunk_count;
    }
}

// PT_PLAYER_JOINED
static void s_receive_packet_player_joined(
    serialiser_t *in_serialiser,
    event_submissions_t *events) {
    packet_player_joined_t packet = {};
    deserialise_player_joined(&packet, in_serialiser);

    LOG_INFOV("%s joined the game\n", packet.player_info.name);

    event_new_player_t *new_player = FL_MALLOC(event_new_player_t, 1);
    client_t *c = &g_net_data.clients.data[packet.player_info.client_id];
    c->name = packet.player_info.name;
    c->client_id = packet.player_info.client_id;

    new_player->info.client_name = c->name;
    new_player->info.client_id = c->client_id;
    new_player->info.ws_position = packet.player_info.ws_position;
    new_player->info.ws_view_direction = packet.player_info.ws_view_direction;
    new_player->info.ws_up_vector = packet.player_info.ws_up_vector;
    new_player->info.default_speed = packet.player_info.default_speed;

    player_flags_t flags = {};
    flags.is_local = 0;

    new_player->info.flags = flags.u32;

    submit_event(ET_NEW_PLAYER, new_player, events);
}

static void s_receive_packet_player_left(
    serialiser_t *in_serialiser,
    event_submissions_t *events) {
    uint16_t disconnected_client = in_serialiser->deserialise_uint16();

    g_net_data.clients.data[disconnected_client].initialised = 0;

    event_player_disconnected_t *data = FL_MALLOC(event_player_disconnected_t, 1);
    data->client_id = disconnected_client;
    submit_event(ET_PLAYER_DISCONNECTED, data, events);

    LOG_INFO("Player disconnected\n");
}

#include <renderer/input.hpp>

// Debugging!
static bool simulate_lag = 0;

// PT_CLIENT_COMMANDS
static void s_send_packet_client_commands() {
    int32_t local_id = translate_client_to_local_id(current_client_id);
    int32_t p_index = wd_get_local_player();
    
    // DEAD by default
    static player_alive_state_t previous_alive_state = PAS_DEAD;

    // Means that world hasn't been initialised yet (could be timing issues when submitting ENTER_SERVER
    // event and send commands interval, so just to make sure, check that player is not NULL)
    if (p_index >= 0) {
        player_t *p = get_player(p_index);
                                               
        if (simulate_lag) {
            p->cached_player_action_count = 0;
        }
        else if (p) {
            client_t *c = &g_net_data.clients[p->client_id];

            packet_client_commands_t packet = {};
            packet.did_correction = c->waiting_on_correction;

            if (previous_alive_state == PAS_DEAD && p->flags.alive_state == PAS_ALIVE) {
                // Means player just spawned
                LOG_INFO("Requesting to spawn\n");
                packet.requested_spawn = 1;
            }
            else {
                packet.requested_spawn = 0;
            }

            previous_alive_state = (player_alive_state_t)p->flags.alive_state;

            packet.command_count = (uint8_t)p->cached_player_action_count;
            packet.actions = LN_MALLOC(player_action_t, packet.command_count);

            for (uint32_t i = 0; i < packet.command_count; ++i) {
                packet.actions[i].bytes = p->cached_player_actions[i].bytes;
                packet.actions[i].dmouse_x = p->cached_player_actions[i].dmouse_x;
                packet.actions[i].dmouse_y = p->cached_player_actions[i].dmouse_y;
                packet.actions[i].dt = p->cached_player_actions[i].dt;
                packet.actions[i].accumulated_dt = p->cached_player_actions[i].accumulated_dt;
                packet.actions[i].tick = p->cached_player_actions[i].tick;
                //LOG_INFOV("Accumulated dt: %f\n", packet.actions[i].accumulated_dt);
            }

            packet.ws_final_position = p->ws_position;
            packet.ws_final_view_direction = p->ws_view_direction;
            packet.ws_final_up_vector = p->ws_up_vector;
            packet.ws_final_velocity = p->ws_velocity;

            packet.player_flags = p->flags.u32;

            accumulated_predicted_modification_t *next_acc = accumulate_history();
            if (next_acc) {
                // Packet will just take from the accumulation stuff
                packet.modified_chunk_count = next_acc->acc_predicted_chunk_mod_count;
                packet.chunk_modifications = next_acc->acc_predicted_modifications;
            }
            else {
                packet.modified_chunk_count = 0;
                packet.chunk_modifications = NULL;
            }

            if (packet.modified_chunk_count) {
#if NET_DEBUG
                printf("\n SENDING COMMANDS: ");
                for (uint32_t i = 0; i < packet.command_count; ++i) {
                    printf("(Tick %llu | adt %f) ", packet.actions[i].tick, packet.actions[i].accumulated_dt);
                }
                printf("\n");
#endif
            }
            
#if NET_DEBUG_VOXEL_INTERPOLATION
            if (packet.modified_chunk_count) {
                printf("\n");
                LOG_INFOV("(Tick %llu) Modified %i chunks\n", (unsigned long long)get_current_tick(), packet.modified_chunk_count);
                for (uint32_t i = 0; i < packet.modified_chunk_count; ++i) {
                    LOG_INFOV("In chunk (%i %i %i): \n", packet.chunk_modifications[i].x, packet.chunk_modifications[i].y, packet.chunk_modifications[i].z);
                    for (uint32_t v = 0; v < packet.chunk_modifications[i].modified_voxels_count; ++v) {
                        LOG_INFOV("- index %i | initial value %i | final value %i\n", (int32_t)packet.chunk_modifications[i].modifications[v].index, (int32_t)packet.chunk_modifications[i].modifications[v].initial_value, (int32_t)packet.chunk_modifications[i].modifications[v].final_value);
                    }
                }
            }
#endif
            
            reset_modification_tracker();
            
            packet_header_t header = {};
            header.current_tick = get_current_tick();
            header.current_packet_count = g_net_data.current_packet;
            header.client_id = current_client_id;
            header.flags.packet_type = PT_CLIENT_COMMANDS;
            header.flags.total_packet_size = packed_packet_header_size() + packed_player_commands_size(&packet);

            serialiser_t serialiser = {};
            serialiser.init(header.flags.total_packet_size);
            serialise_packet_header(&header, &serialiser);
            serialise_player_commands(&packet, &serialiser);

            send_to_game_server(&serialiser, bound_server_address);
    
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
#if 0
            LOG_INFOV("(%i %i %i) Set voxel at index %i to %i\n", cm_ptr->x, cm_ptr->y, cm_ptr->z, vm_ptr->index, (int32_t)vm_ptr->initial_value);
#endif
            c_ptr->voxels[vm_ptr->index] = vm_ptr->initial_value;
        }

        c_ptr->flags.has_to_update_vertices = 1;
    }
}

static void s_revert_accumulated_modifications(
    uint64_t tick_until) {
    // First push all modifications that were done, so that we can revert most previous changes too
    accumulate_history();
    reset_modification_tracker();

    // Starts peeling off from the head
    accumulated_predicted_modification_t *current = g_net_data.acc_predicted_modifications.get_next_item_head();

    uint32_t total_available = g_net_data.acc_predicted_modifications.head_tail_difference;
    uint32_t removed_count = 1;

    uint64_t old_tick = current->tick;
    uint64_t new_tick = 0;
    
    while (current) {
        if (current->tick >= tick_until) {
            // Revert these changes
            s_revert_history_instance(current);
            //LOG_INFOV("- Reverted to tick %llu\n", (unsigned long long)current->tick);

            ++removed_count;

            if (current->tick == tick_until) {
                // Simply break, don't revert anymore
                new_tick = tick_until;
                break;
            }
            else {
                // Peel off next modification
                current = g_net_data.acc_predicted_modifications.get_next_item_head();
            }
        }
        else {
            current = NULL;
            LOG_INFO("BULLLLLLLLLLLLLLSHIIIIIIIIIIIIIIITTTTTTTT ERRRRRRRRROOOOOOORRRRRRRR\n");
            break;
        }
    }
}

static void s_correct_chunks(
    packet_game_state_snapshot_t *snapshot) {
    for (uint32_t cm_index = 0; cm_index < snapshot->modified_chunk_count; ++cm_index) {
        chunk_modifications_t *cm_ptr = &snapshot->chunk_modifications[cm_index];
        chunk_t *c_ptr = get_chunk(ivector3_t(cm_ptr->x, cm_ptr->y, cm_ptr->z));

        //LOG_INFOV("Correcting chunk (%i %i %i)\n", cm_ptr->x, cm_ptr->y, cm_ptr->z);
        for (uint32_t vm_index = 0; vm_index < cm_ptr->modified_voxels_count; ++vm_index) {
            voxel_modification_t *vm_ptr = &cm_ptr->modifications[vm_index];
#if 0
            printf("(%i %i %i) Setting (%i) to %i\n", c_ptr->chunk_coord.x, c_ptr->chunk_coord.y, c_ptr->chunk_coord.z, vm_ptr->index, (int32_t)vm_ptr->final_value);
#endif
            c_ptr->voxels[vm_ptr->index] = vm_ptr->final_value;
        }
    }
}

static void s_handle_incorrect_state(
    client_t *c,
    player_t *p,
    player_snapshot_t *snapshot,
    packet_game_state_snapshot_t *packet,
    serialiser_t *serialiser,
    event_submissions_t *events) {
    // Do correction!
    p->ws_position = snapshot->ws_position;
    p->ws_view_direction = snapshot->ws_view_direction;
    p->ws_up_vector = snapshot->ws_up_vector;
    p->cached_player_action_count = 0;
    p->player_action_count = 0;
    p->accumulated_dt = 0.0f;
    p->next_random_spawn_position = snapshot->ws_next_random_spawn;
    p->ws_velocity = snapshot->ws_velocity;
    p->flags.interaction_mode = snapshot->interaction_mode;

    if (p->flags.alive_state == PAS_ALIVE && snapshot->alive_state == PAS_DEAD) {
        // Handle death
        wd_kill_local_player(events);
    }

    p->flags.alive_state = snapshot->alive_state;

    // Revert voxel modifications up from tick that server processed
    if (snapshot->terraformed) {
#if NET_DEBUG
        LOG_INFOV("Reverting to %llu...\n", (unsigned long long)snapshot->tick);
#endif
        s_revert_accumulated_modifications(snapshot->tick);
        s_correct_chunks(packet);
        // Sets all voxels to what the server has: client should be fully up to date, no need to interpolate between voxels

        // Now deserialise extra voxel corrections
        if (snapshot->packet_contains_terrain_correction) {
            uint32_t modification_count = 0;
            // BUG:
            chunk_modifications_t *modifications = deserialise_chunk_modifications(&modification_count, serialiser);

            for (uint32_t cm_index = 0; cm_index < modification_count; ++cm_index) {
                chunk_modifications_t *cm_ptr = &modifications[cm_index];
                chunk_t *c_ptr = get_chunk(ivector3_t(cm_ptr->x, cm_ptr->y, cm_ptr->z));
                for (uint32_t vm_index = 0; vm_index < cm_ptr->modified_voxels_count; ++vm_index) {
                    voxel_modification_t *vm_ptr = &cm_ptr->modifications[vm_index];
                    c_ptr->voxels[vm_ptr->index] = vm_ptr->final_value;
                }
            }
        }
    }
                
    get_current_tick() = snapshot->tick;

    // Basically says that the client just did a correction - set correction flag on next packet sent to server
    c->waiting_on_correction = 1;
}

static void s_set_voxels_to_final_interpolated_values() {
    chunks_to_interpolate_t *cti_ptr = wd_get_chunks_to_interpolate();
    for (uint32_t cm_index = 0; cm_index < cti_ptr->modification_count; ++cm_index) {
        chunk_modifications_t *cm_ptr = &cti_ptr->modifications[cm_index];

        chunk_t *c_ptr = get_chunk(ivector3_t(cm_ptr->x, cm_ptr->y, cm_ptr->z));

        for (uint32_t vm_index = 0; vm_index < cm_ptr->modified_voxels_count; ++vm_index) {
            voxel_modification_t *vm_ptr = &cm_ptr->modifications[vm_index];
            c_ptr->voxels[vm_ptr->index] = vm_ptr->final_value;
        }

        cm_ptr->modified_voxels_count = 0;
    }

    cti_ptr->modification_count = 0;

    cti_ptr->elapsed = 0.0f;
}

static void s_merge_all_recent_modifications(
    player_snapshot_t *snapshot) {
    uint32_t apm_index = g_net_data.acc_predicted_modifications.tail;
    for (uint32_t apm = 0; apm < g_net_data.acc_predicted_modifications.head_tail_difference; ++apm) {
        accumulated_predicted_modification_t *apm_ptr = &g_net_data.acc_predicted_modifications.buffer[apm_index];
        // For all modifications that were after the snapshot tick that server is sending us
        if (apm_ptr->tick >= snapshot->tick) {
            // Merge modifications
            //LOG_INFOV("Merging with tick %llu\n", apm_ptr->tick);
            merge_chunk_modifications(
                g_net_data.merged_recent_modifications.acc_predicted_modifications,
                &g_net_data.merged_recent_modifications.acc_predicted_chunk_mod_count,
                apm_ptr->acc_predicted_modifications,
                apm_ptr->acc_predicted_chunk_mod_count);
        }

        apm_index = g_net_data.acc_predicted_modifications.increment_index(apm_index);
    }
}

static void s_create_voxels_that_need_to_be_interpolated(
    uint32_t modified_chunk_count,
    chunk_modifications_t *chunk_modifications) {
    chunks_to_interpolate_t *cti_ptr = wd_get_chunks_to_interpolate();
    for (uint32_t recv_cm_index = 0; recv_cm_index < modified_chunk_count; ++recv_cm_index) {
        chunk_modifications_t *recv_cm_ptr = &chunk_modifications[recv_cm_index];
        chunk_t *c_ptr = get_chunk(ivector3_t(recv_cm_ptr->x, recv_cm_ptr->y, recv_cm_ptr->z));

        if (c_ptr->flags.modified_marker) {
            chunk_modifications_t *dst_cm_ptr = &cti_ptr->modifications[cti_ptr->modification_count];

            uint32_t local_cm_index = c_ptr->flags.index_of_modification_struct;
            chunk_modifications_t *local_cm_ptr = &g_net_data.merged_recent_modifications.acc_predicted_modifications[local_cm_index];
            // Chunk was flagged as modified, need to check voxel per voxel if we need to push this to chunks to interpolate
            fill_dummy_voxels(local_cm_ptr);

            uint32_t count = 0;
            for (uint32_t recv_vm_index = 0; recv_vm_index < recv_cm_ptr->modified_voxels_count; ++recv_vm_index) {
                voxel_modification_t *recv_vm_ptr = &recv_cm_ptr->modifications[recv_vm_index];
                if (g_net_data.dummy_voxels[recv_vm_ptr->index] == CHUNK_SPECIAL_VALUE) {
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
                    //LOG_INFO("There is an error\n");
                    // Was modified, should not push
                    // TODO: Think about how to resolve these sort of conflicts
                }
            }

            if (count) {
                ++cti_ptr->modification_count;
            }

            unfill_dummy_voxels(local_cm_ptr);
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
}

static void s_clear_outdated_modifications_from_history(
    player_snapshot_t *snapshot) {
    if (snapshot->terraformed) {
        // Need to remove all modifications from tail to tick
        accumulated_predicted_modification_t *current = g_net_data.acc_predicted_modifications.get_next_item_tail();
        uint32_t count = 1;

        // Pop all modifications until last tick that server processed
        while (current != NULL) {
            if (current->acc_predicted_chunk_mod_count) {
                //LOG_INFOV("Cleared %i chunks\n", current->acc_predicted_chunk_mod_count);
            }
                        
            if (current->tick == snapshot->terraform_tick) {
                //LOG_INFOV("(To tick %llu) Cleared %i, there are %i left\n", (unsigned long long)snapshot->terraform_tick, (int32_t)count, acc_predicted_modifications.head_tail_difference);
                break;
            }
            else if (current->tick > snapshot->terraform_tick) {
                //LOG_ERROR("ERRORRRORORORORRO\n");
            }

            current = g_net_data.acc_predicted_modifications.get_next_item_tail();
            ++count;
        }

        if (current == NULL && count > 1) {
            //LOG_INFO("Didn't clear any\n");
        }
    }
}

static void s_handle_correct_state(
    client_t *c,
    player_t *p,
    player_snapshot_t *snapshot,
    packet_game_state_snapshot_t *packet,
    serialiser_t *serialiser) {
    if (snapshot->terraformed) {
        //LOG_INFOV("Syncing with tick: %llu\n", (unsigned long long)snapshot->terraform_tick);
    }

    if (p) {
        p->next_random_spawn_position = snapshot->ws_next_random_spawn;
    }

    if (still_receiving_chunk_packets){
        accumulated_predicted_modification_t *new_modification = add_acc_predicted_modification();
        acc_predicted_modification_init(new_modification, 0);

        new_modification->acc_predicted_chunk_mod_count = packet->modified_chunk_count;
        for (uint32_t i = 0; i < packet->modified_chunk_count; ++i) {
            chunk_modifications_t *cm_ptr_dst = &new_modification->acc_predicted_modifications[i];
            chunk_modifications_t *cm_ptr_src = &packet->chunk_modifications[i];

            cm_ptr_dst->x = cm_ptr_src->x;
            cm_ptr_dst->y = cm_ptr_src->y;
            cm_ptr_dst->z = cm_ptr_src->z;

            cm_ptr_dst->modified_voxels_count = cm_ptr_src->modified_voxels_count;
            memcpy(cm_ptr_dst->modifications, cm_ptr_src->modifications, sizeof(voxel_modification_t) * cm_ptr_src->modified_voxels_count);
        }
    }
    else {
        // Mark all chunks / voxels that were modified from tick that server just processed, to current tick
        // These voxels should not be interpolated, and just left alone, because client just modified them
        // First make sure to finish interpolation of previous voxels
        s_set_voxels_to_final_interpolated_values();

        // Fill merged recent modifications
        acc_predicted_modification_init(
            &g_net_data.merged_recent_modifications,
            0);

        s_merge_all_recent_modifications(
            snapshot);

        flag_modified_chunks(
            g_net_data.merged_recent_modifications.acc_predicted_modifications,
            g_net_data.merged_recent_modifications.acc_predicted_chunk_mod_count);

        s_create_voxels_that_need_to_be_interpolated(
            packet->modified_chunk_count,
            packet->chunk_modifications);

        unflag_modified_chunks(
            g_net_data.merged_recent_modifications.acc_predicted_modifications,
            g_net_data.merged_recent_modifications.acc_predicted_chunk_mod_count);

        s_clear_outdated_modifications_from_history(snapshot);
    }
}

static void s_handle_local_player_snapshot(
    client_t *c,
    player_t *p,
    player_snapshot_t *snapshot,
    packet_game_state_snapshot_t *packet,
    serialiser_t *serialiser,
    event_submissions_t *events) {
    // TODO: Watch out for this:
    if (snapshot->client_needs_to_correct_state && !snapshot->server_waiting_for_correction) {
        s_handle_incorrect_state(
            c,
            p,
            snapshot,
            packet,
            serialiser,
            events);
    }
    else {
        s_handle_correct_state(
            c,
            p,
            snapshot,
            packet,
            serialiser);
    }
}

// PT_GAME_STATE_SNAPSHOT
static void s_receive_packet_game_state_snapshot(
    serialiser_t *serialiser,
    uint64_t received_tick,
    event_submissions_t *events) {
#if NET_DEBUG
    LOG_INFO("\n\n RECEIVED GAME STATE PACKET\n");
#endif

    packet_game_state_snapshot_t packet = {};
    deserialise_game_state_snapshot(&packet, serialiser);
    packet.chunk_modifications = deserialise_chunk_modifications(&packet.modified_chunk_count, serialiser);

    for (uint32_t i = 0; i < packet.player_data_count; ++i) {
        player_snapshot_t *snapshot = &packet.player_snapshots[i];

        if (snapshot->client_id == current_client_id) {
            client_t *c = &g_net_data.clients[snapshot->client_id];
            int32_t local_id = translate_client_to_local_id(snapshot->client_id);
            player_t *p = get_player(local_id);
            if (p) {
                s_handle_local_player_snapshot(
                    c,
                    p,
                    snapshot,
                    &packet,
                    serialiser,
                    events);
            }
        }
        else {
            int32_t local_id = translate_client_to_local_id(snapshot->client_id);
            player_t *p = get_player(local_id);

            if (p) {
                p->remote_snapshots.push_item(snapshot);
            }
        }
    }
}

// PT_CHUNK_VOXELS
static void s_receive_packet_chunk_voxels(
    serialiser_t *serialiser,
    event_submissions_t *events) {
    uint32_t loaded_chunk_count = serialiser->deserialise_uint32();

    LOG_INFOV("Received %d chunks\n", loaded_chunk_count);

    for (uint32_t c = 0; c < loaded_chunk_count; ++c) {
        int16_t x = serialiser->deserialise_int16();
        int16_t y = serialiser->deserialise_int16();
        int16_t z = serialiser->deserialise_int16();

        chunk_t *chunk = get_chunk(ivector3_t(x, y, z));
        chunk->flags.has_to_update_vertices = 1;

        // Also force update surrounding chunks
        get_chunk(ivector3_t(x + 1, y, z))->flags.has_to_update_vertices = 1;
        get_chunk(ivector3_t(x - 1, y, z))->flags.has_to_update_vertices = 1;
        get_chunk(ivector3_t(x, y + 1, z))->flags.has_to_update_vertices = 1;
        get_chunk(ivector3_t(x, y - 1, z))->flags.has_to_update_vertices = 1;
        get_chunk(ivector3_t(x, y, z + 1))->flags.has_to_update_vertices = 1;
        get_chunk(ivector3_t(x, y, z - 1))->flags.has_to_update_vertices = 1;
        
        get_chunk(ivector3_t(x + 1, y + 1, z + 1))->flags.has_to_update_vertices = 1;
        get_chunk(ivector3_t(x + 1, y + 1, z - 1))->flags.has_to_update_vertices = 1;
        get_chunk(ivector3_t(x + 1, y - 1, z + 1))->flags.has_to_update_vertices = 1;
        get_chunk(ivector3_t(x + 1, y - 1, z - 1))->flags.has_to_update_vertices = 1;

        get_chunk(ivector3_t(x - 1, y + 1, z + 1))->flags.has_to_update_vertices = 1;
        get_chunk(ivector3_t(x - 1, y + 1, z - 1))->flags.has_to_update_vertices = 1;
        get_chunk(ivector3_t(x - 1, y - 1, z + 1))->flags.has_to_update_vertices = 1;
        get_chunk(ivector3_t(x - 1, y - 1, z - 1))->flags.has_to_update_vertices = 1;
        
        for (uint32_t v = 0; v < CHUNK_EDGE_LENGTH * CHUNK_EDGE_LENGTH * CHUNK_EDGE_LENGTH;) {
            uint8_t current_value = serialiser->deserialise_uint8();

            if (current_value == CHUNK_SPECIAL_VALUE) {
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

    chunks_to_receive -= loaded_chunk_count;
}

static void s_check_incoming_game_server_packets(
    event_submissions_t *events) {
    raw_input_t *input = get_raw_input();

#if 0
    if (input->buttons[BT_F].instant) {
        simulate_lag = !simulate_lag;

        if (simulate_lag) {LOG_INFO("Simulating lag\n");}
        else {LOG_INFO("Not simulating lag\n");}
    }
#endif

    // Check packets from game server that we are connected to
    if (client_check_incoming_packets) {
        static float elapsed = 0.0f;
        elapsed += cl_delta_time();
        if (elapsed >= client_command_output_interval) {
            // Send commands to the server
            s_send_packet_client_commands();

            elapsed = 0.0f;
        }

        network_address_t received_address = {};
        int32_t received = receive_from_game_server(
            g_net_data.message_buffer,
            sizeof(char) * NET_MAX_MESSAGE_SIZE,
            &received_address);

        // In future, separate thread will be capturing all these packets
        static const uint32_t MAX_RECEIVED_PER_TICK = 4;
        uint32_t i = 0;

        while (received) {
            serialiser_t in_serialiser = {};
            in_serialiser.data_buffer = (uint8_t *)g_net_data.message_buffer;
            in_serialiser.data_buffer_size = received;

            packet_header_t header = {};
            deserialise_packet_header(&header, &in_serialiser);

            switch(header.flags.packet_type) {

            case PT_CONNECTION_HANDSHAKE: {
                s_receive_packet_connection_handshake(
                    &in_serialiser,
                    events);
                return;
            } break;

            case PT_PLAYER_JOINED: {
                s_receive_packet_player_joined(
                    &in_serialiser,
                    events);
            } break;

            case PT_PLAYER_LEFT: {
                s_receive_packet_player_left(
                    &in_serialiser,
                    events);
            } break;

            case PT_GAME_STATE_SNAPSHOT: {
                s_receive_packet_game_state_snapshot(
                    &in_serialiser,
                    header.current_tick,
                    events);
            } break;

            case PT_CHUNK_VOXELS: {
                s_receive_packet_chunk_voxels(
                    &in_serialiser,
                    events);
            } break;

            default: {
                LOG_INFO("Received unidentifiable packet\n");
            } break;

            }

            if (i < MAX_RECEIVED_PER_TICK) {
                received = receive_from_game_server(
                    g_net_data.message_buffer,
                    sizeof(char) * NET_MAX_MESSAGE_SIZE,
                    &received_address);
            }
            else {
                received = false;
            } 

            ++i;
        }

        if (chunks_to_receive == 0 && still_receiving_chunk_packets) {
            LOG_INFO("Finished receiving chunks\n");
            still_receiving_chunk_packets = 0;

            // Set voxels to be interpolated
            player_snapshot_t dummy {};
            dummy.tick = 0;

            s_merge_all_recent_modifications(&dummy);

            s_create_voxels_that_need_to_be_interpolated(
                g_net_data.merged_recent_modifications.acc_predicted_chunk_mod_count,
                g_net_data.merged_recent_modifications.acc_predicted_modifications);

            g_net_data.acc_predicted_modifications.tail = 0;
            g_net_data.acc_predicted_modifications.head = 0;
            g_net_data.acc_predicted_modifications.head_tail_difference = 0;
        }
    }
}

static void s_tick_client(
    event_submissions_t *events) {
    raw_input_t *input = get_raw_input();
    s_check_incoming_game_server_packets(events);
}

// PT_CONNECTION_REQUEST
static void s_send_packet_connection_request(
    uint32_t ip_address,
    local_client_info_t *info) {
    bound_server_address.port = host_to_network_byte_order(GAME_OUTPUT_PORT_SERVER);
    bound_server_address.ipv4_address = ip_address;

    serialiser_t serialiser = {};
    serialiser.init(100);

    packet_connection_request_t request = {};
    request.name = info->name;
    
    packet_header_t header = {};
    header.flags.packet_type = PT_CONNECTION_REQUEST;
    header.flags.total_packet_size = packed_packet_header_size() + packed_connection_request_size(&request);
    header.current_packet_count = g_net_data.current_packet;

    serialise_packet_header(&header, &serialiser);
    serialise_connection_request(&request, &serialiser);

    if (send_to_game_server(&serialiser, bound_server_address)) {
        LOG_INFO("Success sent connection request\n");
        client_check_incoming_packets = 1;
    }
    else {
        LOG_ERROR("Failed to send connection request\n");
    }
}

// PT_CLIENT_DISCONNECT
static void s_send_packet_client_disconnect() {
    serialiser_t serialiser = {};
    serialiser.init(100);

    packet_header_t header = {};
    header.current_tick = get_current_tick();
    header.current_packet_count = g_net_data.current_packet;
    header.client_id = current_client_id;
    header.flags.packet_type = PT_CLIENT_DISCONNECT;
    header.flags.total_packet_size = packed_packet_header_size();

    serialise_packet_header(&header, &serialiser);
    
    send_to_game_server(&serialiser, bound_server_address);
}

static listener_t net_listener_id;

static void s_net_event_listener(
    void *object,
    event_t *event,
    event_submissions_t *events) {
    switch (event->type) {

    case ET_START_CLIENT: {
        event_start_client_t *data = (event_start_client_t *)event->data;
        s_start_client(data);
        FL_FREE(data);
    } break;

    case ET_REQUEST_REFRESH_SERVER_PAGE: {
        nw_request_available_servers();
    } break;

    case ET_REQUEST_TO_JOIN_SERVER: {
        event_data_request_to_join_server_t *data = (event_data_request_to_join_server_t *)event->data;
        local_client_info_t client_info;
        client_info.name = local_client_info.client_name;

        if (data->server_name) {
            uint32_t *game_server_index = g_net_data.available_servers.name_to_server.get(simple_string_hash(data->server_name));
            if (game_server_index) {
                uint32_t ip_address = g_net_data.available_servers.servers[*game_server_index].ipv4_address;
            
                s_send_packet_connection_request(ip_address, &client_info);
            }
            else {
                LOG_ERRORV("Couldn't find server name: %s\n", data->server_name);
            }
        }
        else if (data->ip_address) {
            uint32_t ip_address = str_to_ipv4_int32(data->ip_address, GAME_OUTPUT_PORT_SERVER, SP_UDP);
            s_send_packet_connection_request(ip_address, &client_info);
        }
        else {
            
        }

        FL_FREE(data);
    } break;

    case ET_LEAVE_SERVER: {
        if (bound_server_address.ipv4_address > 0) {
            // Send to server message
            s_send_packet_client_disconnect();
        
            memset(&bound_server_address, 0, sizeof(bound_server_address));
            memset(g_net_data.clients.data, 0, sizeof(client_t) * g_net_data.clients.max_size);

            client_check_incoming_packets = 0;
        
            LOG_INFO("Disconnecting\n");
        }
    } break;

    case ET_ATTEMPT_SIGN_UP: {
        event_attempt_sign_up_t *data = (event_attempt_sign_up_t *)event->data;

        nw_request_sign_up(data->username, data->password);
    } break;

    case ET_ATTEMPT_LOGIN: {
        event_attempt_login_t *data = (event_attempt_login_t *)event->data;

        nw_request_login(data->username, data->password);
    } break;

    case ET_CLOSED_WINDOW: {
        nw_stop_request_thread();
    } break;

    }
}

void nw_init(event_submissions_t *events) {
    net_listener_id = set_listener_callback(&s_net_event_listener, NULL, events);

    subscribe_to_event(ET_START_CLIENT, net_listener_id, events);
    subscribe_to_event(ET_REQUEST_TO_JOIN_SERVER, net_listener_id, events);
    subscribe_to_event(ET_LEAVE_SERVER, net_listener_id, events);
    subscribe_to_event(ET_REQUEST_REFRESH_SERVER_PAGE, net_listener_id, events);
    subscribe_to_event(ET_ATTEMPT_SIGN_UP, net_listener_id, events);
    subscribe_to_event(ET_CLOSED_WINDOW, net_listener_id, events);
    subscribe_to_event(ET_ATTEMPT_LOGIN, net_listener_id, events);

    socket_api_init();

    g_net_data.message_buffer = FL_MALLOC(char, NET_MAX_MESSAGE_SIZE);

    nw_init_meta_connection();

    g_net_data.available_servers.server_count = 0;
    g_net_data.available_servers.servers = FL_MALLOC(game_server_t, NET_MAX_AVAILABLE_SERVER_COUNT);
    memset(g_net_data.available_servers.servers, 0, sizeof(game_server_t) * NET_MAX_AVAILABLE_SERVER_COUNT);
    g_net_data.available_servers.name_to_server.init();
}

void nw_tick(struct event_submissions_t *events) {
    // check_incoming_meta_server_packets(events);
    // Check for meta server packets
    nw_check_meta_request_status_and_handle(events);

    s_tick_client(events);
}

bool nw_connected_to_server() {
    return bound_server_address.ipv4_address != 0;
}
