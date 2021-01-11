#include "cl_net.hpp"
#include "cl_game_predict.hpp"
#include "cl_game_interp.hpp"
#include "net_socket.hpp"
#include "cl_net_receive.hpp"
#include "vkph_event.hpp"

#include <net_meta.hpp>
#include <net_packets.hpp>
#include <allocators.hpp>
#include <vkph_event_data.hpp>
#include <vkph_events.hpp>
#include <vkph_chunk.hpp>
#include <ux_menu_game.hpp>
#include <net_context.hpp>
#include <net_debug.hpp>

namespace cl {

static bool still_receiving_chunk_packets;
static uint32_t chunks_to_receive;

void prepare_receiving() {
    still_receiving_chunk_packets = 0;
    chunks_to_receive = 0;
}

static void s_fill_enter_server_data(
    net::packet_connection_handshake_t *handshake,
    vkph::event_enter_server_t *data,
    net::context_t *ctx) {
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
            get_local_client_index() = data->local_client_id;
        }
    }
}

// PT_CONNECTION_HANDSHAKE
bool receive_packet_connection_handshake(
    serialiser_t *serialiser,
    uint32_t server_tag,
    vkph::state_t *state,
    net::context_t *ctx,
    net::game_server_t *server) {
    net::packet_connection_handshake_t handshake = {};
    handshake.deserialise(serialiser);

    if (handshake.success) {
        // Initialise the teams on the client side
        state->set_teams(handshake.team_count, handshake.team_infos);
        state->current_map_data.view_info = handshake.mvi;

        LOG_INFOV("Received handshake, there are %i players\n", handshake.player_count);

        // Dispatch event to initialise all the players
        auto *data = FL_MALLOC(vkph::event_enter_server_t, 1);
        data->info_count = handshake.player_count;
        data->infos = FL_MALLOC(vkph::player_init_info_t, data->info_count);

        ctx->tag = handshake.client_tag;
        server->tag = server_tag;

        s_fill_enter_server_data(&handshake, data, ctx);

        vkph::submit_event(vkph::ET_ENTER_SERVER, data);

        if (handshake.loaded_chunk_count) {
            still_receiving_chunk_packets = 1;
            chunks_to_receive = handshake.loaded_chunk_count;
        }

        return true;
    }
    else {
        return false;
    }
}

// PT_PLAYER_JOINED
void receive_packet_player_joined(
    serialiser_t *in_serialiser,
    vkph::state_t *state,
    net::context_t *ctx) {
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

// PT_PLAYER_LEFT
void receive_packet_player_left(
    serialiser_t *in_serialiser,
    vkph::state_t *state,
    net::context_t *ctx) {
    uint16_t disconnected_client = in_serialiser->deserialise_uint16();

    ctx->clients.data[disconnected_client].initialised = 0;

    auto *data = FL_MALLOC(vkph::event_player_disconnected_t, 1);
    data->client_id = disconnected_client;
    vkph::submit_event(vkph::ET_PLAYER_DISCONNECTED, data);

    LOG_INFO("Player disconnected\n");
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
    vkph::state_t *state,
    net::context_t *ctx) {
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
    chunks_to_interpolate_t *cti_ptr = get_chunks_to_interpolate();
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
    vkph::state_t *state,
    net::context_t *ctx) {
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
    vkph::state_t *state,
    net::context_t *ctx) {
    chunks_to_interpolate_t *cti_ptr = get_chunks_to_interpolate();
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
                        dst_cm_ptr->modifications[dst_cm_ptr->modified_voxels_count].final_value = recv_vm_ptr->final_value;
                        dst_cm_ptr->colors[dst_cm_ptr->modified_voxels_count++] = recv_vm_ptr->color;
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
                    vkph::voxel_color_t *dst_color = &dst_cm_ptr->colors[vm_index];
                    net::voxel_modification_t *recv_vm_ptr = &recv_cm_ptr->modifications[vm_index];
                    dst_vm_ptr->index = recv_vm_ptr->index;
                    // The initial value is juste the current local value of the chunk
                    dst_vm_ptr->initial_value = c_ptr->voxels[recv_vm_ptr->index].value;
                    dst_vm_ptr->final_value = recv_vm_ptr->final_value;
                    *dst_color = recv_vm_ptr->color;
                }
            }
        }
    }
}

static void s_clear_outdated_modifications_from_history(
    vkph::player_snapshot_t *snapshot,
    net::context_t *ctx) {
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

        if (rock_snapshot->client_id != get_local_client_index()) {
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
    vkph::state_t *state,
    net::context_t *ctx) {
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
        kill_local_player(state);
    }

    p->flags.is_alive = snapshot->alive_state;

    // Revert voxel modifications up from tick that server processed
    if (snapshot->terraformed)  {
        net::debug_log("Reverting all modifications from current to tick %lu\n", ctx->log_file, 0, snapshot->tick);

        s_revert_accumulated_modifications(snapshot->tick, state, ctx);
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
        finish_interp_step(state);

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
    vkph::state_t *state,
    net::context_t *ctx) {
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

        s_merge_all_recent_modifications(snapshot, state, ctx);

        state->flag_modified_chunks(
            ctx->merged_recent_modifications.acc_predicted_modifications,
            ctx->merged_recent_modifications.acc_predicted_chunk_mod_count);

        s_create_voxels_that_need_to_be_interpolated(
            packet->modified_chunk_count,
            packet->chunk_modifications,
            state,
            ctx);

        state->unflag_modified_chunks(
            ctx->merged_recent_modifications.acc_predicted_modifications,
            ctx->merged_recent_modifications.acc_predicted_chunk_mod_count);

        s_clear_outdated_modifications_from_history(snapshot, ctx);
    }
}

static void s_handle_local_player_snapshot(
    net::client_t *c,
    vkph::player_t *p,
    vkph::player_snapshot_t *snapshot,
    net::packet_game_state_snapshot_t *packet,
    serialiser_t *serialiser,
    vkph::state_t *state,
    net::context_t *ctx) {
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
            state,
            ctx);
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
            state,
            ctx);
    }
}

// PT_GAME_STATE_SNAPSHOT
void receive_packet_game_state_snapshot(
    serialiser_t *serialiser,
    uint64_t received_tick,
    vkph::state_t *state,
    net::context_t *ctx) {
    net::debug_log("##### Received game state snapshot\n", ctx->log_file, 0);

    net::packet_game_state_snapshot_t packet = {};
    packet.deserialise(serialiser);
    packet.chunk_modifications = deserialise_chunk_modifications(&packet.modified_chunk_count, serialiser, net::CST_SERIALISE_UNION_COLOR);

    for (uint32_t i = 0; i < packet.player_data_count; ++i) {
        vkph::player_snapshot_t *snapshot = &packet.player_snapshots[i];

        if (snapshot->client_id == get_local_client_index()) {
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
                    state,
                    ctx);
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
void receive_packet_chunk_voxels(
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
            uint32_t debug = v;

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

void receive_player_team_change(
    serialiser_t *serialiser,
    vkph::state_t *state) {
    net::packet_player_team_change_t packet = {};
    packet.deserialise(serialiser);

    // If client ID == local client ID, don't do anything
    // Otherwise, update ui roster and add player to team

    if (packet.client_id != get_local_client_index()) {
        int32_t p_id = state->get_local_id(packet.client_id);

        state->change_player_team(state->get_player(p_id), (vkph::team_color_t)packet.color);

        // Update the text on the game menu
        ux::init_game_menu_for_server(state);
    }
}

void receive_ping(
    serialiser_t *in_serialiser,
    vkph::state_t *state,
    net::context_t *ctx,
    net::game_server_t *server_addr) {
    net::packet_header_t header = {};
    header.current_tick = state->current_tick;
    header.current_packet_count = ctx->current_packet;
    header.client_id = get_local_client_index();
    header.flags.packet_type = net::PT_PING;
    header.flags.total_packet_size = header.size();
    header.tag = ctx->tag;

    serialiser_t serialiser = {};
    serialiser.init(header.flags.total_packet_size);
    header.serialise(&serialiser);

    ctx->main_udp_send_to(&serialiser, server_addr->ipv4_address);
}

void check_if_finished_recv_chunks(vkph::state_t *state, net::context_t *ctx) {
    if (chunks_to_receive == 0 && still_receiving_chunk_packets) {
        LOG_INFO("Finished receiving chunks\n");
        still_receiving_chunk_packets = 0;

        // Set voxels to be interpolated
        vkph::player_snapshot_t dummy {};
        dummy.tick = 0;

        s_merge_all_recent_modifications(&dummy, state, ctx);

        s_create_voxels_that_need_to_be_interpolated(
            ctx->merged_recent_modifications.acc_predicted_chunk_mod_count,
            ctx->merged_recent_modifications.acc_predicted_modifications,
            state,
            ctx);

        ctx->accumulated_modifications.tail = 0;
        ctx->accumulated_modifications.head = 0;
        ctx->accumulated_modifications.head_tail_difference = 0;
    }
}

}
