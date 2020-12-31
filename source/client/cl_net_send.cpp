#include "cl_net_send.hpp"
#include "cl_game_predict.hpp"
#include <vkph_player.hpp>
#include <log.hpp>
#include "cl_net.hpp"
#include "net_socket.hpp"
#include "cl_net_receive.hpp"

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
    net::packet_client_commands_t *packet,
    net::context_t *ctx) {
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
    vkph::state_t *state,
    net::context_t *ctx) {
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
void send_packet_client_commands(vkph::state_t *state, net::context_t *ctx, net::address_t server_addr, bool simulate_lag) {
    int32_t local_id = state->get_local_id(get_local_client_index());
    int32_t p_index = get_local_player(state);
    
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
            s_fill_commands_with_actions(p, &packet, ctx);

            // Fill predicted state
            s_fill_predicted_data(p, &packet, state);
            packet.prediction.player_flags = p->flags;

            // Fill with chunk modifications that were made during past few frames
            s_fill_with_accumulated_chunk_modifications(&packet, state, ctx);
            state->reset_modification_tracker();
                        
            net::packet_header_t header = {};
            { // Fill header
                header.current_tick = state->current_tick;
                header.current_packet_count = ctx->current_packet;
                header.client_id = get_local_client_index();
                header.flags.packet_type = net::PT_CLIENT_COMMANDS;
                header.flags.total_packet_size = header.size() + packet.size();
            }

            serialiser_t serialiser = {};
            { // Serialise all the data
                serialiser.init(header.flags.total_packet_size);
                header.serialise(&serialiser);
                packet.serialise(&serialiser);
            }

            ctx->main_udp_send_to(&serialiser, server_addr);
    
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

// PT_CONNECTION_REQUEST
bool send_packet_connection_request(
    uint32_t ip_address,
    local_client_info_t *info,
    net::context_t *ctx,
    net::address_t *server_addr) {
    server_addr->port = net::host_to_network_byte_order(net::GAME_OUTPUT_PORT_SERVER);
    server_addr->ipv4_address = ip_address;

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

    if (ctx->main_udp_send_to(&serialiser, *server_addr)) {
        LOG_INFO("Success sent connection request\n");
        return true;
    }
    else {
        LOG_ERROR("Failed to send connection request\n");
        return false;
    }
}

// PT_TEAM_SELECT_REQUEST
void send_packet_team_select_request(
    vkph::team_color_t color,
    const vkph::state_t *state,
    net::context_t *ctx,
    net::address_t server_addr) {
    serialiser_t serialiser = {};
    serialiser.init(100);

    net::packet_header_t header = {};
    header.current_tick = state->current_tick;
    header.current_packet_count = ctx->current_packet;
    header.client_id = get_local_client_index();
    header.flags.packet_type = net::PT_TEAM_SELECT_REQUEST;
    header.flags.total_packet_size = header.size() + sizeof(uint32_t);

    header.serialise(&serialiser);
    serialiser.serialise_uint32((uint32_t)color);

    ctx->main_udp_send_to(&serialiser, server_addr);
}

// PT_CLIENT_DISCONNECT
void send_packet_client_disconnect(const vkph::state_t *state, net::context_t *ctx, net::address_t server_addr) {
    serialiser_t serialiser = {};
    serialiser.init(100);

    net::packet_header_t header = {};
    header.current_tick = state->current_tick;
    header.current_packet_count = ctx->current_packet;
    header.client_id = get_local_client_index();
    header.flags.packet_type = net::PT_CLIENT_DISCONNECT;
    header.flags.total_packet_size = header.size();

    header.serialise(&serialiser);
    
    ctx->main_udp_send_to(&serialiser, server_addr);
}

}
