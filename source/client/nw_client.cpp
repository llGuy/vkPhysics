#include "cl_main.hpp"
#include "net_meta.hpp"
#include "ui_game_menu.hpp"
#include <vkph_chunk.hpp>
#include <vkph_player.hpp>
#include <vkph_weapon.hpp>
#include "nw_client.hpp"
#include "wd_interp.hpp"
#include "wd_predict.hpp"
#include <vkph_state.hpp>
#include <vkph_events.hpp>
#include <vkph_event_data.hpp>
#include <string.hpp>
#include "nw_client_meta.hpp"
#include <constant.hpp>
#include <cstddef>

#include <net_debug.hpp>
#include <net_context.hpp>

#include <app.hpp>

static net::context_t *ctx;

static bool started_client = 0;
static bool client_check_incoming_packets = 0;
static bool still_receiving_chunk_packets;

static uint16_t current_client_id;
static uint32_t chunks_to_receive;

static net::address_t bound_server_address = {};

// Start the client sockets and initialize containers
static void s_start_client(
    vkph::event_start_client_t *data,
    vkph::state_t *state) {
    still_receiving_chunk_packets = 0;
    chunks_to_receive = 0;

    memset(ctx->dummy_voxels, vkph::CHUNK_SPECIAL_VALUE, sizeof(ctx->dummy_voxels));
    ctx->init_main_udp_socket(net::GAME_OUTPUT_PORT_CLIENT);
    ctx->clients.init(net::NET_MAX_CLIENT_COUNT);
    started_client = 1;
    state->flags.track_history = 1;
    ctx->accumulated_modifications.init();

    uint32_t sizeof_chunk_mod_pack = sizeof(net::chunk_modifications_t) * net::MAX_PREDICTED_CHUNK_MODIFICATIONS;

    ctx->chunk_modification_allocator.pool_init(
        sizeof_chunk_mod_pack,
        sizeof_chunk_mod_pack * (net::NET_MAX_ACCUMULATED_PREDICTED_CHUNK_MODIFICATIONS_PER_PACK + 4));

    //s_acc_predicted_modification_init(&merged_recent_modifications, 0);
    ctx->merged_recent_modifications.tick = 0;
    ctx->merged_recent_modifications.acc_predicted_chunk_mod_count = 0;
    ctx->merged_recent_modifications.acc_predicted_modifications = FL_MALLOC(
        net::chunk_modifications_t,
        net::NET_MAX_ACCUMULATED_PREDICTED_CHUNK_MODIFICATIONS_PER_PACK);
}

static void s_fill_enter_server_data(
    net::packet_connection_handshake_t *handshake,
    vkph::event_enter_server_t *data) {
    int32_t highest_client_index = -1;
    
    // Add all the players
    for (uint32_t i = 0; i < handshake->player_count; ++i) {
        if (highest_client_index < (int32_t)handshake->player_infos[i].client_id) {
            highest_client_index = handshake->player_infos[i].client_id;
        }

        uint16_t client_id = handshake->player_infos[i].client_id;

        /*
          Fill data in the net's clients array.
        */
        ctx->clients.data[client_id].client_id = client_id;
        ctx->clients.data[client_id].name = handshake->player_infos[i].client_name;
        ctx->clients.data[client_id].initialised = 1;

        /*
          Fill the player init infos so that local client can initialise the world.
        */
        data->infos[i] = handshake->player_infos[i];
        data->infos[i].client_name = ctx->clients.data[client_id].name;
        data->infos[i].client_id = client_id;

        vkph::player_flags_t pflags = {};
        pflags.u32 = handshake->player_infos[i].flags;

        if (pflags.is_local) {
            ctx->clients.data[client_id].chunks_to_wait_for = handshake->loaded_chunk_count;

            data->local_client_id = ctx->clients.data[i].client_id;
            current_client_id = data->local_client_id;
        }
    }
}

// PT_CONNECTION_HANDSHAKE
static void s_receive_packet_connection_handshake(
    serialiser_t *serialiser,
    vkph::state_t *state) {
    net::packet_connection_handshake_t handshake = {};
    handshake.deserialise(serialiser);

    // Initialise the teams on the client side
    state->set_teams(handshake.team_count, handshake.team_infos);

    LOG_INFOV("Received handshake, there are %i players\n", handshake.player_count);

    // Dispatch event to initialise all the players
    auto *data = FL_MALLOC(vkph::event_enter_server_t, 1);
    data->info_count = handshake.player_count;
    data->infos = FL_MALLOC(vkph::player_init_info_t, data->info_count);

    s_fill_enter_server_data(&handshake, data);

    vkph::submit_event(vkph::ET_ENTER_SERVER, data);

    if (handshake.loaded_chunk_count) {
        still_receiving_chunk_packets = 1;
        chunks_to_receive = handshake.loaded_chunk_count;
    }
}

// PT_PLAYER_JOINED
static void s_receive_packet_player_joined(
    serialiser_t *in_serialiser,
    vkph::state_t *state) {
    net::packet_player_joined_t packet = {};
    packet.deserialise(in_serialiser);

    LOG_INFOV("%s joined the game\n", packet.player_info.client_name);

    auto *new_player = FL_MALLOC(vkph::event_new_player_t, 1);
    net::client_t *c = &ctx->clients.data[packet.player_info.client_id];
    c->name = packet.player_info.client_name;
    c->client_id = packet.player_info.client_id;

    new_player->info = packet.player_info;

    vkph::player_flags_t flags = {};
    flags.is_local = 0;

    new_player->info.flags = flags.u32;

    vkph::submit_event(vkph::ET_NEW_PLAYER, new_player);
}

static void s_receive_packet_player_left(
    serialiser_t *in_serialiser,
    vkph::state_t *state) {
    uint16_t disconnected_client = in_serialiser->deserialise_uint16();

    ctx->clients.data[disconnected_client].initialised = 0;

    auto *data = FL_MALLOC(vkph::event_player_disconnected_t, 1);
    data->client_id = disconnected_client;
    vkph::submit_event(vkph::ET_PLAYER_DISCONNECTED, data);

    LOG_INFO("Player disconnected\n");
}

#include <app.hpp>

// Debugging!
static bool simulate_lag = 0;

static void s_inform_on_death(
    vkph::player_t *p,
    bool was_alive,
    net::packet_client_commands_t *packet) {
    if (!was_alive && p->flags.is_alive) {
        // Means player just spawned
        LOG_INFO("Requesting to spawn\n");
        packet->requested_spawn = 1;
    }
    else {
        packet->requested_spawn = 0;
    }
}

static void s_fill_commands_with_actions(
    vkph::player_t *p,
    net::packet_client_commands_t *packet) {
    packet->command_count = (uint8_t)p->cached_player_action_count;
    packet->actions = LN_MALLOC(vkph::player_action_t, packet->command_count);

    if (packet->command_count) {
        net::debug_log("\tClient has made %d actions\n", ctx->log_file, 0, packet->command_count);
    }

    for (uint32_t i = 0; i < packet->command_count; ++i) {
        packet->actions[i].bytes = p->cached_player_actions[i].bytes;
        packet->actions[i].dmouse_x = p->cached_player_actions[i].dmouse_x;
        packet->actions[i].dmouse_y = p->cached_player_actions[i].dmouse_y;
        packet->actions[i].dt = p->cached_player_actions[i].dt;
        packet->actions[i].accumulated_dt = p->cached_player_actions[i].accumulated_dt;
        packet->actions[i].tick = p->cached_player_actions[i].tick;

        net::debug_log(
            "\t\tAt tick %lu: actions %d, dmouse_x %f; dmouse_y %f; dt %f; accumulated dt %f\n",
            ctx->log_file,
            0,
            packet->actions[i].tick,
            packet->actions[i].bytes,
            packet->actions[i].dmouse_x,
            packet->actions[i].dmouse_y,
            packet->actions[i].dt,
            packet->actions[i].accumulated_dt);
    }

    if (packet->command_count) {
        net::debug_log("\tWith these actions, predicted values were:\n", ctx->log_file, 0);
        net::debug_log("\t\tPosition: %f %f %f\n", ctx->log_file, 0, p->ws_position.x, p->ws_position.y, p->ws_position.z);
        net::debug_log("\t\tVelocity: %f %f %f\n", ctx->log_file, 0, p->ws_velocity.x, p->ws_velocity.y, p->ws_velocity.z);
        net::debug_log("\t\tView Direction: %f %f %f\n", ctx->log_file, 0, p->ws_view_direction.x, p->ws_view_direction.y, p->ws_view_direction.z);
        net::debug_log("\t\tUp vector: %f %f %f\n", ctx->log_file, 0, p->ws_up_vector.x, p->ws_up_vector.y, p->ws_up_vector.z);
    }
}

static void s_fill_predicted_data(
    vkph::player_t *p,
    net::packet_client_commands_t *packet,
    vkph::state_t *state) {
    packet->prediction.ws_position = p->ws_position;
    packet->prediction.ws_view_direction = p->ws_view_direction;
    packet->prediction.ws_up_vector = p->ws_up_vector;
    packet->prediction.ws_velocity = p->ws_velocity;

    /* 
      Add the players that the local client thinks it has hit with projectiles
      Don't need to add the new bullets / rocks that the client spawns
      Because if it so happens that the client was cheating, when the client
      thinks there was a hit, the server will know that it was incorrect because
      the bullet wouldn't have been spawned on server-side
    */
    packet->predicted_hit_count = state->predicted_hits.data_count;
    packet->hits = LN_MALLOC(vkph::predicted_projectile_hit_t, packet->predicted_hit_count);

    uint32_t actual_predicted_count = 0;

    for (uint32_t i = 0; i < packet->predicted_hit_count; ++i) {
        if (state->predicted_hits[i].flags.initialised) {
            packet->hits[actual_predicted_count++] = state->predicted_hits[i];
        }
    }

    packet->predicted_hit_count = actual_predicted_count;
}

static void s_fill_with_accumulated_chunk_modifications(
    net::packet_client_commands_t *packet,
    vkph::state_t *state) {
    net::accumulated_predicted_modification_t *next_acc = ctx->accumulate_history(state);
    if (next_acc) {
        // Packet will just take from the accumulation stuff
        packet->prediction.chunk_mod_count = next_acc->acc_predicted_chunk_mod_count;
        packet->prediction.chunk_modifications = next_acc->acc_predicted_modifications;
    }
    else {
        packet->prediction.chunk_mod_count = 0;
        packet->prediction.chunk_modifications = NULL;
    }

    if (packet->prediction.chunk_mod_count) {
        net::debug_log("\tModified %i chunks\n", ctx->log_file, 0, packet->prediction.chunk_mod_count);
        for (uint32_t i = 0; i < packet->prediction.chunk_mod_count; ++i) {
            net::debug_log(
                "\t\tIn chunk (%i %i %i):\n",
                ctx->log_file,
                0,
                packet->prediction.chunk_modifications[i].x,
                packet->prediction.chunk_modifications[i].y,
                packet->prediction.chunk_modifications[i].z);

            for (uint32_t v = 0; v < packet->prediction.chunk_modifications[i].modified_voxels_count; ++v) {
                net::debug_log(
                    "\t\t- index %i \t| initial value %i \t| final value %i\n",
                    ctx->log_file,
                    0,
                    (int32_t)packet->prediction.chunk_modifications[i].modifications[v].index,
                    (int32_t)packet->prediction.chunk_modifications[i].modifications[v].initial_value,
                    (int32_t)packet->prediction.chunk_modifications[i].modifications[v].final_value);
            }
        }
    }
}

// PT_CLIENT_COMMANDS
static void s_send_packet_client_commands(vkph::state_t *state) {
    int32_t local_id = state->get_local_id(current_client_id);
    int32_t p_index = wd_get_local_player();
    
    // DEAD by default
    static bool was_alive = false;

    // Means that world hasn't been initialised yet (could be timing issues when submitting ENTER_SERVER
    // event and send commands interval, so just to make sure, check that player is not NULL)
    if (p_index >= 0) {
        vkph::player_t *p = state->get_player(p_index);
                                               
        if (simulate_lag) {
            p->cached_player_action_count = 0;
            net::debug_log("@@@@@@ ^^^ We are simulating lag so not sending anything\n", ctx->log_file, 0);
        }
        else if (p) {
            net:: client_t *c = &ctx->clients[p->client_id];

            net::packet_client_commands_t packet = {};
            packet.did_correction = c->waiting_on_correction;

            // Tell server if player just died and update the "previous alive state" variable
            s_inform_on_death(p, was_alive, &packet);
            was_alive = p->flags.is_alive;

            // Fill packet with the actions
            s_fill_commands_with_actions(p, &packet);

            // Fill predicted state
            s_fill_predicted_data(p, &packet, state);
            packet.prediction.player_flags = p->flags;

            // Fill with chunk modifications that were made during past few frames
            s_fill_with_accumulated_chunk_modifications(&packet, state);
            state->reset_modification_tracker();
                        
            net::packet_header_t header = {};
            { // Fill header
                header.current_tick = state->current_tick;
                header.current_packet_count = ctx->current_packet;
                header.client_id = current_client_id;
                header.flags.packet_type = net::PT_CLIENT_COMMANDS;
                header.flags.total_packet_size = header.size() + packet.size();
            }

            serialiser_t serialiser = {};
            { // Serialise all the data
                serialiser.init(header.flags.total_packet_size);
                header.serialise(&serialiser);
                packet.serialise(&serialiser);
            }

            ctx->main_udp_send_to(&serialiser, bound_server_address);
    
            p->cached_player_action_count = 0;
            c->waiting_on_correction = 0;

            // Clear projectiles
            for (uint32_t i = 0; i < state->predicted_hits.data_count; ++i) {
                state->predicted_hits[i].flags.initialised = 0;
            }

            state->predicted_hits.clear();
            state->rocks.clear_recent();
        }
    }
}

// Just for one accumulated_predicted_modification_t structure
static void s_revert_history_instance(
    net::accumulated_predicted_modification_t *apm_ptr,
    vkph::state_t *state) {
    for (uint32_t cm_index = 0; cm_index < apm_ptr->acc_predicted_chunk_mod_count; ++cm_index) {
        net::chunk_modifications_t *cm_ptr = &apm_ptr->acc_predicted_modifications[cm_index];
        vkph::chunk_t *c_ptr = state->get_chunk(ivector3_t(cm_ptr->x, cm_ptr->y, cm_ptr->z));

        for (uint32_t vm_index = 0; vm_index < cm_ptr->modified_voxels_count; ++vm_index) {
            net::voxel_modification_t *vm_ptr = &cm_ptr->modifications[vm_index];
            // Set all modified to initial values
#if 0
            LOG_INFOV("(%i %i %i) Set voxel at index %i to %i\n", cm_ptr->x, cm_ptr->y, cm_ptr->z, vm_ptr->index, (int32_t)vm_ptr->initial_value);
#endif
            c_ptr->voxels[vm_ptr->index].value = vm_ptr->initial_value;
            c_ptr->voxels[vm_ptr->index].color = cm_ptr->colors[vm_ptr->index];
        }

        c_ptr->flags.has_to_update_vertices = 1;
    }
}

static void s_revert_accumulated_modifications(
    uint64_t tick_until,
    vkph::state_t *state) {
    // First push all modifications that were done, so that we can revert most previous changes too
    ctx->accumulate_history(state);
    state->reset_modification_tracker();

    // Starts peeling off from the head
    net::accumulated_predicted_modification_t *current = ctx->accumulated_modifications.get_next_item_head();

    if (current) {
        uint32_t total_available = ctx->accumulated_modifications.head_tail_difference;
        uint32_t removed_count = 1;

        uint64_t old_tick = current->tick;
        uint64_t new_tick = 0;
    
        while (current) {
            if (current->tick >= tick_until) {
                // Revert these changes
                s_revert_history_instance(current, state);
                //LOG_INFOV("- Reverted to tick %llu\n", (unsigned long long)current->tick);

                ++removed_count;

                if (current->tick == tick_until) {
                    // Simply break, don't revert anymore
                    new_tick = tick_until;
                    break;
                }
                else {
                    // Peel off next modification
                    current = ctx->accumulated_modifications.get_next_item_head();
                }
            }
            else {
                current = NULL;
                LOG_INFO("BULLLLLLLLLLLLLLSHIIIIIIIIIIIIIIITTTTTTTT ERRRRRRRRROOOOOOORRRRRRRR\n");
                break;
            }
        }
}}

static void s_correct_chunks(
    net::packet_game_state_snapshot_t *snapshot,
    vkph::state_t *state) {
    for (uint32_t cm_index = 0; cm_index < snapshot->modified_chunk_count; ++cm_index) {
        net::chunk_modifications_t *cm_ptr = &snapshot->chunk_modifications[cm_index];
        vkph::chunk_t *c_ptr = state->get_chunk(ivector3_t(cm_ptr->x, cm_ptr->y, cm_ptr->z));

        //LOG_INFOV("Correcting chunk (%i %i %i)\n", cm_ptr->x, cm_ptr->y, cm_ptr->z);
        for (uint32_t vm_index = 0; vm_index < cm_ptr->modified_voxels_count; ++vm_index) {
            net::voxel_modification_t *vm_ptr = &cm_ptr->modifications[vm_index];
#if 0
            printf("(%i %i %i) Setting (%i) to %i\n", c_ptr->chunk_coord.x, c_ptr->chunk_coord.y, c_ptr->chunk_coord.z, vm_ptr->index, (int32_t)vm_ptr->final_value);
#endif
            c_ptr->voxels[vm_ptr->index].value = vm_ptr->final_value;
        }
    }
}

static void s_set_voxels_to_final_interpolated_values(vkph::state_t *state) {
    chunks_to_interpolate_t *cti_ptr = wd_get_chunks_to_interpolate();
    for (uint32_t cm_index = 0; cm_index < cti_ptr->modification_count; ++cm_index) {
        net::chunk_modifications_t *cm_ptr = &cti_ptr->modifications[cm_index];

        vkph::chunk_t *c_ptr = state->get_chunk(ivector3_t(cm_ptr->x, cm_ptr->y, cm_ptr->z));

        for (uint32_t vm_index = 0; vm_index < cm_ptr->modified_voxels_count; ++vm_index) {
            net::voxel_modification_t *vm_ptr = &cm_ptr->modifications[vm_index];
            c_ptr->voxels[vm_ptr->index].value = vm_ptr->final_value;
        }

        cm_ptr->modified_voxels_count = 0;
    }

    cti_ptr->modification_count = 0;

    cti_ptr->elapsed = 0.0f;
}

static void s_merge_all_recent_modifications(
    vkph::player_snapshot_t *snapshot,
    vkph::state_t *state) {
    uint32_t apm_index = ctx->accumulated_modifications.tail;
    for (uint32_t apm = 0; apm < ctx->accumulated_modifications.head_tail_difference; ++apm) {
        net::accumulated_predicted_modification_t *apm_ptr = &ctx->accumulated_modifications.buffer[apm_index];
        // For all modifications that were after the snapshot tick that server is sending us
        if (apm_ptr->tick >= snapshot->tick) {
            // Merge modifications
            //LOG_INFOV("Merging with tick %llu\n", apm_ptr->tick);
            ctx->merge_chunk_modifications(
                ctx->merged_recent_modifications.acc_predicted_modifications,
                &ctx->merged_recent_modifications.acc_predicted_chunk_mod_count,
                apm_ptr->acc_predicted_modifications,
                apm_ptr->acc_predicted_chunk_mod_count,
                state);
        }

        apm_index = ctx->accumulated_modifications.increment_index(apm_index);
    }
}

static void s_create_voxels_that_need_to_be_interpolated(
    uint32_t modified_chunk_count,
    net::chunk_modifications_t *chunk_modifications,
    vkph::state_t *state) {
    chunks_to_interpolate_t *cti_ptr = wd_get_chunks_to_interpolate();
    for (uint32_t recv_cm_index = 0; recv_cm_index < modified_chunk_count; ++recv_cm_index) {
        net::chunk_modifications_t *recv_cm_ptr = &chunk_modifications[recv_cm_index];
        vkph::chunk_t *c_ptr = state->get_chunk(ivector3_t(recv_cm_ptr->x, recv_cm_ptr->y, recv_cm_ptr->z));

        if (c_ptr->flags.modified_marker) {
            net::chunk_modifications_t *dst_cm_ptr = &cti_ptr->modifications[cti_ptr->modification_count];

            uint32_t local_cm_index = c_ptr->flags.index_of_modification_struct;
            net::chunk_modifications_t *local_cm_ptr = &ctx->merged_recent_modifications.acc_predicted_modifications[local_cm_index];
            // Chunk was flagged as modified, need to check voxel per voxel if we need to push this to chunks to interpolate
            ctx->fill_dummy_voxels(local_cm_ptr);

            uint32_t count = 0;
            for (uint32_t recv_vm_index = 0; recv_vm_index < recv_cm_ptr->modified_voxels_count; ++recv_vm_index) {
                net::voxel_modification_t *recv_vm_ptr = &recv_cm_ptr->modifications[recv_vm_index];
                if (ctx->dummy_voxels[recv_vm_ptr->index] == vkph::CHUNK_SPECIAL_VALUE) {
                    if (recv_vm_ptr->final_value != c_ptr->voxels[recv_vm_ptr->index].value) {
                        // Was not modified, can push this
                        dst_cm_ptr->modifications[dst_cm_ptr->modified_voxels_count].index = recv_vm_ptr->index;
                        // Initial value is current value of voxel
                        dst_cm_ptr->modifications[dst_cm_ptr->modified_voxels_count].initial_value = c_ptr->voxels[recv_vm_ptr->index].value;
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

            ctx->unfill_dummy_voxels(local_cm_ptr);
        }
        else {
            // Simple push this to chunks to interpolate
            net::chunk_modifications_t *dst_cm_ptr = &cti_ptr->modifications[cti_ptr->modification_count];

            if (recv_cm_ptr->modified_voxels_count) {
                ++cti_ptr->modification_count;

                dst_cm_ptr->x = recv_cm_ptr->x;
                dst_cm_ptr->y = recv_cm_ptr->y;
                dst_cm_ptr->z = recv_cm_ptr->z;
                dst_cm_ptr->modified_voxels_count = recv_cm_ptr->modified_voxels_count;

                //memcpy(dst_cm_ptr->modifications, recv_cm_ptr->modifications, sizeof(voxel_modification_t) * dst_cm_ptr->modified_voxels_count);
                // Need to change initial value to current voxel values
                for (uint32_t vm_index = 0; vm_index < dst_cm_ptr->modified_voxels_count; ++vm_index) {
                    net::voxel_modification_t *dst_vm_ptr = &dst_cm_ptr->modifications[vm_index];
                    net::voxel_modification_t *recv_vm_ptr = &recv_cm_ptr->modifications[vm_index];
                    dst_vm_ptr->index = recv_vm_ptr->index;
                    dst_vm_ptr->initial_value = c_ptr->voxels[recv_vm_ptr->index].value;
                    dst_vm_ptr->final_value = recv_vm_ptr->final_value;
                }
            }
        }
    }
}

static void s_clear_outdated_modifications_from_history(
    vkph::player_snapshot_t *snapshot) {
    if (snapshot->terraformed) {
        // Need to remove all modifications from tail to tick
        net::accumulated_predicted_modification_t *current = ctx->accumulated_modifications.get_next_item_tail();
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

            current = ctx->accumulated_modifications.get_next_item_tail();
            ++count;
        }

        if (current == NULL && count > 1) {
            //LOG_INFO("Didn't clear any\n");
        }
    }
}

static void s_add_projectiles_from_snapshot(
    net::packet_game_state_snapshot_t *snapshot,
    vkph::state_t *state) {
    for (uint32_t i = 0; i < snapshot->rock_count; ++i) {
        vkph::rock_snapshot_t *rock_snapshot = &snapshot->rock_snapshots[i];

        if (rock_snapshot->client_id != current_client_id) {
            // Spawn this rock
            state->rocks.spawn(
                rock_snapshot->position,
                rock_snapshot->direction,
                rock_snapshot->up,
                rock_snapshot->client_id,
                // We don't care about ref indices
                0, 0);
        }
    }
}

static void s_handle_incorrect_state(
    net::client_t *c,
    vkph::player_t *p,
    vkph::player_snapshot_t *snapshot,
    net::packet_game_state_snapshot_t *packet,
    serialiser_t *serialiser,
    vkph::state_t *state) {
    vkph::player_flags_t real_player_flags = {};
    real_player_flags.u32 = snapshot->player_local_flags;

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
    p->animated_state = (vkph::player_animated_state_t)snapshot->animated_state;
    p->frame_displacement = snapshot->frame_displacement;
    p->flags.is_on_ground = real_player_flags.is_on_ground;
    p->flags.moving = real_player_flags.moving;

    net::debug_log("\t\tSetting position to (%f %f %f)\n", ctx->log_file, 0, p->ws_position.x, p->ws_position.y, p->ws_position.z);
    net::debug_log("\t\tSetting velocity to (%f %f %f)\n", ctx->log_file, 0, p->ws_velocity.x, p->ws_velocity.y, p->ws_velocity.z);
    net::debug_log("\t\tSetting view direction to (%f %f %f)\n", ctx->log_file, 0, p->ws_view_direction.x, p->ws_view_direction.y, p->ws_view_direction.z);
    net::debug_log("\t\tSetting up vector to (%f %f %f)\n", ctx->log_file, 0, p->ws_up_vector.x, p->ws_up_vector.y, p->ws_up_vector.z);

    if (p->flags.is_alive && !snapshot->alive_state) {
        // Handle death
        wd_kill_local_player(state);
    }

    p->flags.is_alive = snapshot->alive_state;

    // Revert voxel modifications up from tick that server processed
    if (snapshot->terraformed)  {
        net::debug_log("Reverting all modifications from current to tick %lu\n", ctx->log_file, 0, snapshot->tick);

        s_revert_accumulated_modifications(snapshot->tick, state);
        s_correct_chunks(packet, state);
        // Sets all voxels to what the server has: client should be fully up to date, no need to interpolate between voxels

        // Now deserialise extra voxel corrections
        if (snapshot->packet_contains_terrain_correction) {
            uint32_t modification_count = 0;

            net::chunk_modifications_t *modifications = deserialise_chunk_modifications(&modification_count, serialiser, net::CST_SERIALISE_UNION_COLOR);

            for (uint32_t cm_index = 0; cm_index < modification_count; ++cm_index) {
                net::chunk_modifications_t *cm_ptr = &modifications[cm_index];
                vkph::chunk_t *c_ptr = state->get_chunk(ivector3_t(cm_ptr->x, cm_ptr->y, cm_ptr->z));
                for (uint32_t vm_index = 0; vm_index < cm_ptr->modified_voxels_count; ++vm_index) {
                    net::voxel_modification_t *vm_ptr = &cm_ptr->modifications[vm_index];
                    c_ptr->voxels[vm_ptr->index].value = vm_ptr->final_value;
                    // Color will not be stored in the separate color array
                    c_ptr->voxels[vm_ptr->index].color = vm_ptr->color;
                }
            }
        }
    }
    else {
        LOG_INFOV("Hard-sync chunks with server's chunks (%d)\n", packet->modified_chunk_count);

        // Need to finish all interpolation of chunks
        wd_finish_interp_step(state);

        for (uint32_t cm_index = 0; cm_index < packet->modified_chunk_count; ++cm_index) {
            net::chunk_modifications_t *cm_ptr = &packet->chunk_modifications[cm_index];
            vkph::chunk_t *c_ptr = state->get_chunk(ivector3_t(cm_ptr->x, cm_ptr->y, cm_ptr->z));

            for (uint32_t v_index = 0; v_index < cm_ptr->modified_voxels_count; ++v_index) {
                net::voxel_modification_t *vm_ptr = &cm_ptr->modifications[v_index];
                c_ptr->voxels[vm_ptr->index].value = vm_ptr->final_value;
                c_ptr->voxels[vm_ptr->index].color = vm_ptr->color;
            }
        }
    }
                
    state->current_tick = snapshot->tick;

    // Basically says that the client just did a correction - set correction flag on next packet sent to server
    c->waiting_on_correction = 1;
}

static void s_handle_correct_state(
    net::client_t *c,
    vkph::player_t *p,
    vkph::player_snapshot_t *snapshot,
    net::packet_game_state_snapshot_t *packet,
    serialiser_t *serialiser,
    vkph::state_t *state) {
    if (snapshot->terraformed) {
        //LOG_INFOV("Syncing with tick: %llu\n", (unsigned long long)snapshot->terraform_tick);
    }

    if (p) {
        p->next_random_spawn_position = snapshot->ws_next_random_spawn;
    }

    if (still_receiving_chunk_packets){
        net::accumulated_predicted_modification_t *new_modification = ctx->add_acc_predicted_modification();
        ctx->acc_predicted_modification_init(new_modification, 0);

        new_modification->acc_predicted_chunk_mod_count = packet->modified_chunk_count;
        for (uint32_t i = 0; i < packet->modified_chunk_count; ++i) {
            net::chunk_modifications_t *cm_ptr_dst = &new_modification->acc_predicted_modifications[i];
            net::chunk_modifications_t *cm_ptr_src = &packet->chunk_modifications[i];

            cm_ptr_dst->x = cm_ptr_src->x;
            cm_ptr_dst->y = cm_ptr_src->y;
            cm_ptr_dst->z = cm_ptr_src->z;

            cm_ptr_dst->modified_voxels_count = cm_ptr_src->modified_voxels_count;
            memcpy(cm_ptr_dst->modifications, cm_ptr_src->modifications, sizeof(net::voxel_modification_t) * cm_ptr_src->modified_voxels_count);
        }
    }
    else {
        // Mark all chunks / voxels that were modified from tick that server just processed, to current tick
        // These voxels should not be interpolated, and just left alone, because client just modified them
        // First make sure to finish interpolation of previous voxels
        s_set_voxels_to_final_interpolated_values(state);

        // Fill merged recent modifications
        ctx->acc_predicted_modification_init(&ctx->merged_recent_modifications, 0);

        s_merge_all_recent_modifications(snapshot, state);

        state->flag_modified_chunks(
            ctx->merged_recent_modifications.acc_predicted_modifications,
            ctx->merged_recent_modifications.acc_predicted_chunk_mod_count);

        s_create_voxels_that_need_to_be_interpolated(
            packet->modified_chunk_count,
            packet->chunk_modifications,
            state);

        state->unflag_modified_chunks(
            ctx->merged_recent_modifications.acc_predicted_modifications,
            ctx->merged_recent_modifications.acc_predicted_chunk_mod_count);

        s_clear_outdated_modifications_from_history(snapshot);
    }
}

static void s_handle_local_player_snapshot(
    net::client_t *c,
    vkph::player_t *p,
    vkph::player_snapshot_t *snapshot,
    net::packet_game_state_snapshot_t *packet,
    serialiser_t *serialiser,
    vkph::state_t *state) {
    s_add_projectiles_from_snapshot(packet, state);

    // TODO: Watch out for this:
    if (snapshot->client_needs_to_correct_state && !snapshot->server_waiting_for_correction) {
        net::debug_log("\tClient has to do a correction\n", ctx->log_file, 1);

        s_handle_incorrect_state(
            c,
            p,
            snapshot,
            packet,
            serialiser,
            state);
    }
    else {
        net::debug_log("\tClient predicted state correctly - no correction needed!\n", ctx->log_file, 0);

        if (snapshot->server_waiting_for_correction) {
            LOG_INFO("Server is waiting for the correction\n");
        }

        s_handle_correct_state(
            c,
            p,
            snapshot,
            packet,
            serialiser,
            state);
    }
}

// PT_GAME_STATE_SNAPSHOT
static void s_receive_packet_game_state_snapshot(
    serialiser_t *serialiser,
    uint64_t received_tick,
    vkph::state_t *state) {
    net::debug_log("##### Received game state snapshot\n", ctx->log_file, 0);

    net::packet_game_state_snapshot_t packet = {};
    packet.deserialise(serialiser);
    packet.chunk_modifications = deserialise_chunk_modifications(&packet.modified_chunk_count, serialiser, net::CST_SERIALISE_UNION_COLOR);

    for (uint32_t i = 0; i < packet.player_data_count; ++i) {
        vkph::player_snapshot_t *snapshot = &packet.player_snapshots[i];

        if (snapshot->client_id == current_client_id) {
            net::client_t *c = &ctx->clients[snapshot->client_id];
            int32_t local_id = state->get_local_id(snapshot->client_id);
            auto *p = state->get_player(local_id);
            if (p) {
                s_handle_local_player_snapshot(
                    c,
                    p,
                    snapshot,
                    &packet,
                    serialiser,
                    state);
            }
        }
        else {
            int32_t local_id = state->get_local_id(snapshot->client_id);
            auto *p = state->get_player(local_id);

            if (p) {
                p->remote_snapshots.push_item(snapshot);
            }
        }
    }
}

// PT_CHUNK_VOXELS
static void s_receive_packet_chunk_voxels(
    serialiser_t *serialiser,
    vkph::state_t *state) {
    uint32_t loaded_chunk_count = serialiser->deserialise_uint32();

    for (uint32_t c = 0; c < loaded_chunk_count; ++c) {
        int16_t x = serialiser->deserialise_int16();
        int16_t y = serialiser->deserialise_int16();
        int16_t z = serialiser->deserialise_int16();

        vkph::chunk_t *chunk = state->get_chunk(ivector3_t(x, y, z));
        chunk->flags.has_to_update_vertices = 1;
        
        for (uint32_t v = 0; v < vkph::CHUNK_EDGE_LENGTH * vkph::CHUNK_EDGE_LENGTH * vkph::CHUNK_EDGE_LENGTH;) {
            uint8_t current_value = serialiser->deserialise_uint8();
            uint8_t current_color = serialiser->deserialise_uint8();

            if (current_value == vkph::CHUNK_SPECIAL_VALUE) {
                chunk->voxels[v].value = 0;
                chunk->voxels[v].color = 0;
                ++v;

                // Repeating zeros
                uint32_t zero_count = serialiser->deserialise_uint32();
                chunk->voxels[v + 1].value = 0;
                chunk->voxels[v + 1].color = 0;
                chunk->voxels[v + 2].value = 0;
                chunk->voxels[v + 2].color = 0;

                v += 2;

                uint32_t previous_v = v;
                for (; v < previous_v + zero_count - 3; ++v) {
                    chunk->voxels[v].value = 0;
                    chunk->voxels[v].color = 0;
                }
            }
            else {
                chunk->voxels[v].value = current_value;
                chunk->voxels[v].color = current_color;
                ++v;
            }
        }
    }

    uint32_t loaded;
    vkph::chunk_t **chunks = state->get_active_chunks(&loaded);
    LOG_INFOV("Currently there are %d loaded chunks\n", loaded);

    chunks_to_receive -= loaded_chunk_count;

    if (chunks_to_receive == 0) {
        for (uint32_t i = 0; i < loaded; ++i) {
            vkph::chunk_t *c = state->chunks[i];
            uint32_t x = c->chunk_coord.x;
            uint32_t y = c->chunk_coord.y;
            uint32_t z = c->chunk_coord.z;

            vkph::chunk_t *a = NULL;
            if ((a = state->access_chunk(ivector3_t(x + 1, y, z)))) a->flags.has_to_update_vertices = 1;
            if ((a = state->access_chunk(ivector3_t(x - 1, y, z)))) a->flags.has_to_update_vertices = 1;
            if ((a = state->access_chunk(ivector3_t(x, y + 1, z)))) a->flags.has_to_update_vertices = 1;
            if ((a = state->access_chunk(ivector3_t(x, y - 1, z)))) a->flags.has_to_update_vertices = 1;
            if ((a = state->access_chunk(ivector3_t(x, y, z + 1)))) a->flags.has_to_update_vertices = 1;
            if ((a = state->access_chunk(ivector3_t(x, y, z - 1)))) a->flags.has_to_update_vertices = 1;
            if ((a = state->access_chunk(ivector3_t(x + 1, y + 1, z + 1)))) a->flags.has_to_update_vertices = 1;
            if ((a = state->access_chunk(ivector3_t(x + 1, y + 1, z - 1)))) a->flags.has_to_update_vertices = 1;
            if ((a = state->access_chunk(ivector3_t(x + 1, y - 1, z + 1)))) a->flags.has_to_update_vertices = 1;
            if ((a = state->access_chunk(ivector3_t(x + 1, y - 1, z - 1)))) a->flags.has_to_update_vertices = 1;
            if ((a = state->access_chunk(ivector3_t(x - 1, y + 1, z + 1)))) a->flags.has_to_update_vertices = 1;
            if ((a = state->access_chunk(ivector3_t(x - 1, y + 1, z - 1)))) a->flags.has_to_update_vertices = 1;
            if ((a = state->access_chunk(ivector3_t(x - 1, y - 1, z + 1)))) a->flags.has_to_update_vertices = 1;
            if ((a = state->access_chunk(ivector3_t(x - 1, y - 1, z - 1)))) a->flags.has_to_update_vertices = 1;
        }
    }

    state->get_active_chunks(&loaded);
    LOG_INFOV("Currently there are %d loaded chunks\n", loaded);
}

static void s_receive_player_team_change(
    serialiser_t *serialiser,
    vkph::state_t *state) {
    net::packet_player_team_change_t packet = {};
    packet.deserialise(serialiser);

    // If client ID == local client ID, don't do anything
    // Otherwise, update ui roster and add player to team

    if (packet.client_id != nw_get_local_client_index()) {
        int32_t p_id = state->get_local_id(packet.client_id);

        state->change_player_team(state->get_player(p_id), (vkph::team_color_t)packet.color);

        // Update the text on the game menu
        ui_init_game_menu_for_server(state);
    }
}

static void s_receive_ping(
    serialiser_t *in_serialiser,
    vkph::state_t *state) {
    net::packet_header_t header = {};
    header.current_tick = state->current_tick;
    header.current_packet_count = ctx->current_packet;
    header.client_id = current_client_id;
    header.flags.packet_type = net::PT_PING;
    header.flags.total_packet_size = header.size();

    serialiser_t serialiser = {};
    serialiser.init(header.flags.total_packet_size);
    header.serialise(&serialiser);

    ctx->main_udp_send_to(&serialiser, bound_server_address);
}

static void s_check_incominstate_server_packets(
    vkph::state_t *state) {
    const app::raw_input_t *input = app::get_raw_input();

#if 1
    if (input->buttons[app::BT_F].instant) {
        simulate_lag = !simulate_lag;

        if (simulate_lag) {LOG_INFO("Simulating lag\n");}
        else {LOG_INFO("Not simulating lag\n");}
    }
#endif

    // Check packets from game server that we are connected to
    if (client_check_incoming_packets) {
        static float elapsed = 0.0f;
        elapsed += cl_delta_time();
        if (elapsed >= net::NET_CLIENT_COMMAND_OUTPUT_INTERVAL) {
            // Send commands to the server
            net::debug_log("----- Sending client commands to the server at tick %llu\n", ctx->log_file, 0, state->current_tick);
            s_send_packet_client_commands(state);

            elapsed = 0.0f;
        }

        net::address_t received_address = {};
        int32_t received = ctx->main_udp_recv_from(
            ctx->message_buffer,
            sizeof(char) * net::NET_MAX_MESSAGE_SIZE,
            &received_address);

        // In future, separate thread will be capturing all these packets
        static const uint32_t MAX_RECEIVED_PER_TICK = 4;
        uint32_t i = 0;

        while (received) {
            serialiser_t in_serialiser = {};
            in_serialiser.data_buffer = (uint8_t *)ctx->message_buffer;
            in_serialiser.data_buffer_size = received;

            net::packet_header_t header = {};
            header.deserialise(&in_serialiser);

            switch(header.flags.packet_type) {

            case net::PT_CONNECTION_HANDSHAKE: {
                s_receive_packet_connection_handshake(
                    &in_serialiser,
                    state);
                return;
            } break;

            case net::PT_PLAYER_JOINED: {
                s_receive_packet_player_joined(
                    &in_serialiser,
                    state);
            } break;

            case net::PT_PLAYER_LEFT: {
                s_receive_packet_player_left(
                    &in_serialiser,
                    state);
            } break;

            case net::PT_GAME_STATE_SNAPSHOT: {
                s_receive_packet_game_state_snapshot(
                    &in_serialiser,
                    header.current_tick,
                    state);
            } break;

            case net::PT_CHUNK_VOXELS: {
                s_receive_packet_chunk_voxels(
                    &in_serialiser,
                    state);
            } break;

            case net::PT_PLAYER_TEAM_CHANGE: {
                s_receive_player_team_change(
                    &in_serialiser,
                    state);
            } break;

            case net::PT_PING: {
                s_receive_ping(
                    &in_serialiser,
                    state);
            } break;

            default: {
                LOG_INFO("Received unidentifiable packet\n");
            } break;

            }

            if (i < MAX_RECEIVED_PER_TICK) {
                received = ctx->main_udp_recv_from(
                    ctx->message_buffer,
                    sizeof(char) * net::NET_MAX_MESSAGE_SIZE,
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
            vkph::player_snapshot_t dummy {};
            dummy.tick = 0;

            s_merge_all_recent_modifications(&dummy, state);

            s_create_voxels_that_need_to_be_interpolated(
                ctx->merged_recent_modifications.acc_predicted_chunk_mod_count,
                ctx->merged_recent_modifications.acc_predicted_modifications,
                state);

            ctx->accumulated_modifications.tail = 0;
            ctx->accumulated_modifications.head = 0;
            ctx->accumulated_modifications.head_tail_difference = 0;
        }
    }
}

static void s_tick_client(vkph::state_t *state) {
    s_check_incominstate_server_packets(state);
}

// PT_CONNECTION_REQUEST
static void s_send_packet_connection_request(
    uint32_t ip_address,
    local_client_info_t *info) {
    bound_server_address.port = net::host_to_network_byte_order(net::GAME_OUTPUT_PORT_SERVER);
    bound_server_address.ipv4_address = ip_address;

    serialiser_t serialiser = {};
    serialiser.init(100);

    net::packet_connection_request_t request = {};
    request.name = info->name;
    
    net::packet_header_t header = {};
    header.flags.packet_type = net::PT_CONNECTION_REQUEST;
    header.flags.total_packet_size = header.size() + request.size();
    header.current_packet_count = ctx->current_packet;

    header.serialise(&serialiser);
    request.serialise(&serialiser);

    if (ctx->main_udp_send_to(&serialiser, bound_server_address)) {
        LOG_INFO("Success sent connection request\n");
        client_check_incoming_packets = 1;
    }
    else {
        LOG_ERROR("Failed to send connection request\n");
    }
}

// PT_TEAM_SELECT_REQUEST
static void s_send_packet_team_select_request(vkph::team_color_t color, const vkph::state_t *state) {
    serialiser_t serialiser = {};
    serialiser.init(100);

    net::packet_header_t header = {};
    header.current_tick = state->current_tick;
    header.current_packet_count = ctx->current_packet;
    header.client_id = current_client_id;
    header.flags.packet_type = net::PT_TEAM_SELECT_REQUEST;
    header.flags.total_packet_size = header.size() + sizeof(uint32_t);

    header.serialise(&serialiser);
    serialiser.serialise_uint32((uint32_t)color);

    ctx->main_udp_send_to(&serialiser, bound_server_address);
}

// PT_CLIENT_DISCONNECT
static void s_send_packet_client_disconnect(const vkph::state_t *state) {
    serialiser_t serialiser = {};
    serialiser.init(100);

    net::packet_header_t header = {};
    header.current_tick = state->current_tick;
    header.current_packet_count = ctx->current_packet;
    header.client_id = current_client_id;
    header.flags.packet_type = net::PT_CLIENT_DISCONNECT;
    header.flags.total_packet_size = header.size();

    header.serialise(&serialiser);
    
    ctx->main_udp_send_to(&serialiser, bound_server_address);
}

static vkph::listener_t net_listener_id;

static void s_net_event_listener(
    void *object,
    vkph::event_t *event) {
    auto *state = (vkph::state_t *)object;

    switch (event->type) {

    case vkph::ET_START_CLIENT: {
        auto *data = (vkph::event_start_client_t *)event->data;
        s_start_client(data, state);
        FL_FREE(data);
    } break;

    case vkph::ET_REQUEST_REFRESH_SERVER_PAGE: {
        nw_request_available_servers();
    } break;

    case vkph::ET_REQUEST_TO_JOIN_SERVER: {
        auto *local_meta_client = nw_get_local_meta_client();

        auto *data = (vkph::event_data_request_to_join_server_t *)event->data;
        local_client_info_t client_info;
        client_info.name = local_meta_client->username;

        if (data->server_name) {
            uint32_t *game_server_index = ctx->available_servers.name_to_server.get(simple_string_hash(data->server_name));
            if (game_server_index) {
                uint32_t ip_address = ctx->available_servers.servers[*game_server_index].ipv4_address;
            
                s_send_packet_connection_request(ip_address, &client_info);
            }
            else {
                LOG_ERRORV("Couldn't find server name: %s\n", data->server_name);
            }
        }
        else if (data->ip_address) {
            uint32_t ip_address = str_to_ipv4_int32(data->ip_address, net::GAME_OUTPUT_PORT_SERVER, net::SP_UDP);
            s_send_packet_connection_request(ip_address, &client_info);
        }
        else {
            
        }

        FL_FREE(data);
    } break;

    case vkph::ET_LEAVE_SERVER: {
        if (bound_server_address.ipv4_address > 0) {
            // Send to server message
            s_send_packet_client_disconnect(state);
        
            memset(&bound_server_address, 0, sizeof(bound_server_address));
            memset(ctx->clients.data, 0, sizeof(net::client_t) * ctx->clients.max_size);

            client_check_incoming_packets = 0;
        
            LOG_INFO("Disconnecting\n");
        }
    } break;

    case vkph::ET_ATTEMPT_SIGN_UP: {
        auto *data = (vkph::event_attempt_sign_up_t *)event->data;

        nw_request_sign_up(data->username, data->password);
    } break;

    case vkph::ET_ATTEMPT_LOGIN: {
        auto *data = (vkph::event_attempt_login_t *)event->data;

        nw_request_login(data->username, data->password);
    } break;

    case vkph::ET_SEND_SERVER_TEAM_SELECT_REQUEST: {
        auto *data = (vkph::event_send_server_team_select_request_t *)event->data;

        s_send_packet_team_select_request(data->color, state);
    } break;

    case vkph::ET_CLOSED_WINDOW: {
        nw_notify_meta_disconnection();
    } break;

    }
}

void nw_init(vkph::state_t *state) {
    ctx = flmalloc<net::context_t>();

    net_listener_id = vkph::set_listener_callback(&s_net_event_listener, state);

    vkph::subscribe_to_event(vkph::ET_START_CLIENT, net_listener_id);
    vkph::subscribe_to_event(vkph::ET_REQUEST_TO_JOIN_SERVER, net_listener_id);
    vkph::subscribe_to_event(vkph::ET_LEAVE_SERVER, net_listener_id);
    vkph::subscribe_to_event(vkph::ET_REQUEST_REFRESH_SERVER_PAGE, net_listener_id);
    vkph::subscribe_to_event(vkph::ET_ATTEMPT_SIGN_UP, net_listener_id);
    vkph::subscribe_to_event(vkph::ET_CLOSED_WINDOW, net_listener_id);
    vkph::subscribe_to_event(vkph::ET_ATTEMPT_LOGIN, net_listener_id);
    vkph::subscribe_to_event(vkph::ET_SEND_SERVER_TEAM_SELECT_REQUEST, net_listener_id);

    net::init_socket_api();

    ctx->message_buffer = FL_MALLOC(char, net::NET_MAX_MESSAGE_SIZE);

    nw_init_meta_connection();

    ctx->available_servers.server_count = 0;
    ctx->available_servers.servers = FL_MALLOC(net::game_server_t, net::NET_MAX_AVAILABLE_SERVER_COUNT);
    memset(ctx->available_servers.servers, 0, sizeof(net::game_server_t) * net::NET_MAX_AVAILABLE_SERVER_COUNT);
    ctx->available_servers.name_to_server.init();
}

void nw_tick(vkph::state_t *state) {
    // check_incoming_meta_server_packets(events);
    // Check for meta server packets
    nw_check_meta_request_status_and_handle();

    s_tick_client(state);
}

bool nw_connected_to_server() {
    return bound_server_address.ipv4_address != 0;
}

uint16_t nw_get_local_client_index() {
    return current_client_id;
}

net::available_servers_t *nw_get_available_servers() {
    return &ctx->available_servers;
}
