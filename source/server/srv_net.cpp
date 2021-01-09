#include "srv_net.hpp"
#include "allocators.hpp"
#include "net_socket.hpp"
#include "srv_net_meta.hpp"
#include "constant.hpp"
#include <vkph_player.hpp>
#include "t_types.hpp"
#include <vkph_team.hpp>
#include <vkph_weapon.hpp>
#include "srv_main.hpp"
#include "srv_game.hpp"
#include <vkph_state.hpp>
#include <vkph_events.hpp>
#include <string.hpp>
#include <cstddef>
#include <vkph_event_data.hpp>
#include <vkph_chunk.hpp>
#include <vkph_physics.hpp>

#include <net_context.hpp>
#include <net_packets.hpp>

namespace srv {

/*
  Net code main state:
*/
static net::context_t *ctx;

static flexible_stack_container_t<uint32_t> clients_to_send_chunks_to;
static hash_table_t<uint16_t, 50, 5, 5> client_tag_to_id;

// Local server information
struct server_info_t {
    const char *server_name;
};

static server_info_t local_server_info;
static bool started_server = 0;

/*
  When we've accepted a new connection, we need to temporarily store it
  until we have received the connection request, and actually created the client.
 */
struct pending_connection_t {
    bool pending;
    net::socket_t s;
    float elapsed_time;
    net::address_t addr;
};

static static_stack_container_t<pending_connection_t, 50> pending_conns;

static uint32_t s_generate_tag() {
    return rand();
}

static void s_start_server(vkph::event_start_server_t *data, vkph::state_t *state) {
    clients_to_send_chunks_to.init(50);

    memset(ctx->dummy_voxels, vkph::CHUNK_SPECIAL_VALUE, sizeof(ctx->dummy_voxels));

    ctx->init_main_udp_socket(net::GAME_OUTPUT_PORT_SERVER);

    ctx->clients.init(net::NET_MAX_CLIENT_COUNT);

    started_server = 1;

    state->flags.track_history = 1;

    ctx->accumulated_modifications.init();

    uint32_t sizeof_chunk_mod_pack = sizeof(net::chunk_modifications_t) * net::MAX_PREDICTED_CHUNK_MODIFICATIONS;

    ctx->chunk_modification_allocator.pool_init(
        sizeof_chunk_mod_pack,
        sizeof_chunk_mod_pack * net::NET_MAX_ACCUMULATED_PREDICTED_CHUNK_MODIFICATIONS_PER_PACK + 4);

    client_tag_to_id.init();
    ctx->tag = s_generate_tag();

    { // Create main TCP socket
        ctx->main_tcp_socket = net::network_socket_init(net::SP_TCP);

        net::address_t addr = {};
        addr.port = net::host_to_network_byte_order(net::GAME_SERVER_LISTENING_PORT);
        net::bind_network_socket_to_port(ctx->main_tcp_socket, addr);
        net::set_socket_blocking_state(ctx->main_tcp_socket, 0);
        net::set_socket_to_listening(ctx->main_tcp_socket, 50);
    }

    pending_conns.init();
}

// PT_CONNECTION_HANDSHAKE
static bool s_send_packet_connection_handshake(
    uint16_t client_id,
    vkph::event_new_player_t *player_info,
    uint32_t loaded_chunk_count,
    const vkph::state_t *state) {
    uint32_t new_client_tag = s_generate_tag();

    net::packet_connection_handshake_t connection_handshake = {};
    // TODO: Make sure to set this to 0 if the server is full
    connection_handshake.success = 1;
    connection_handshake.loaded_chunk_count = loaded_chunk_count;
    connection_handshake.mvi.pos = state->current_map_data.view_info.pos;
    connection_handshake.mvi.dir = state->current_map_data.view_info.dir;
    connection_handshake.mvi.up = state->current_map_data.view_info.up;
    connection_handshake.client_tag = new_client_tag;

    LOG_INFOV("Loaded chunk count: %d\n", connection_handshake.loaded_chunk_count);

    connection_handshake.player_infos = LN_MALLOC(vkph::player_init_info_t, ctx->clients.data_count);

    { // Fill in the data on the players
        for (uint32_t i = 0; i < ctx->clients.data_count; ++i) {
            net::client_t *client = ctx->clients.get(i);
            if (client->initialised) {
                vkph::player_init_info_t *info = &connection_handshake.player_infos[connection_handshake.player_count];

                if (i == client_id) {
                    *info = player_info->info;
                    info->client_name = client->name;
                    info->client_id = client->client_id;

                    vkph::player_flags_t flags = {};
                    flags.u32 = player_info->info.flags;
                    flags.is_local = 1;
                    flags.team_color = vkph::team_color_t::INVALID;

                    info->flags = flags.u32;
                }
                else {
                    int32_t local_id = state->get_local_id(i);
                    const vkph::player_t *p = state->get_player(local_id);

                    p->make_player_init_info(info);
                    info->client_name = client->name;
                    info->client_id = client->client_id;

                    vkph::player_flags_t flags = {};
                    flags.u32 = p->flags.u32;
                    flags.is_local = 0;
                
                    info->flags = flags.u32;
                }
            
                ++connection_handshake.player_count;
            }
        }
    }

    { // Fill in data on teams
        uint32_t team_count = state->team_count;
        const vkph::team_t *teams = state->teams;

        connection_handshake.team_count = team_count;
        connection_handshake.team_infos = LN_MALLOC(vkph::team_info_t, team_count);
        for (uint32_t i = 0; i < team_count; ++i) {
            connection_handshake.team_infos[i] = teams[i].make_team_info();
        }
    }

    net::packet_header_t header = {};
    header.current_tick = state->current_tick;
    header.flags.total_packet_size = header.size() + connection_handshake.size();
    header.flags.packet_type = net::PT_CONNECTION_HANDSHAKE;
    header.tag = ctx->tag;

    serialiser_t serialiser = {};
    serialiser.init(header.flags.total_packet_size);

    header.serialise(&serialiser);
    connection_handshake.serialise(&serialiser);

    net::client_t *c = ctx->clients.get(client_id);
    c->client_tag = new_client_tag;
    client_tag_to_id.insert(new_client_tag, client_id);

    return net::send_to_bound_address(c->tcp_socket, (char *)serialiser.data_buffer, serialiser.data_buffer_head);
}

static constexpr uint32_t s_maximum_chunks_per_packet() {
    return ((65507 - sizeof(uint32_t)) / (sizeof(int16_t) * 3 + vkph::CHUNK_BYTE_SIZE));
}

static bool s_serialise_chunk(
    serialiser_t *serialiser,
    uint32_t *chunks_in_packet,
    net::voxel_chunk_values_t *values,
    uint32_t i) {
    net::voxel_chunk_values_t *current_values = &values[i];

    uint32_t before_chunk_ptr = serialiser->data_buffer_head;

    serialiser->serialise_int16(current_values->x);
    serialiser->serialise_int16(current_values->y);
    serialiser->serialise_int16(current_values->z);

    // Do a compression of the chunk values
    for (uint32_t v_index = 0; v_index < vkph::CHUNK_VOXEL_COUNT; ++v_index) {
        uint32_t debug = v_index;

        vkph::voxel_t current_voxel = current_values->voxel_values[v_index];
        if (current_voxel.value == 0) {
            uint32_t before_head = serialiser->data_buffer_head;

            static constexpr uint32_t MAX_ZERO_COUNT_BEFORE_COMPRESSION = 3;

            uint32_t zero_count = 0;
            for (; current_values->voxel_values[v_index].value == 0 && zero_count < MAX_ZERO_COUNT_BEFORE_COMPRESSION; ++v_index, ++zero_count) {
                serialiser->serialise_uint8(0);
                serialiser->serialise_uint8(0);
            }

            if (zero_count == MAX_ZERO_COUNT_BEFORE_COMPRESSION) {
                for (; current_values->voxel_values[v_index].value == 0; ++v_index, ++zero_count) {}

                if (zero_count >= vkph::CHUNK_VOXEL_COUNT) {
                    serialiser->data_buffer_head = before_chunk_ptr;
                    
                    return 0;
                }

                serialiser->data_buffer_head = before_head;
                serialiser->serialise_uint8(vkph::CHUNK_SPECIAL_VALUE);
                serialiser->serialise_uint8(vkph::CHUNK_SPECIAL_VALUE);
                serialiser->serialise_uint32(zero_count);
            }

            v_index -= 1;
        }
        else {
            serialiser->serialise_uint8(current_voxel.value);
            serialiser->serialise_uint8(current_voxel.color);
        }
    }

    *chunks_in_packet = *chunks_in_packet + 1;

    return 1;
}

// PT_CHUNK_VOXELS
static uint32_t s_prepare_packet_chunk_voxels(
    net::client_t *client,
    net::voxel_chunk_values_t *values,
    uint32_t count,
    const vkph::state_t *state) {
    net::packet_header_t header = {};
    header.flags.packet_type = net::PT_CHUNK_VOXELS;
    header.flags.total_packet_size = s_maximum_chunks_per_packet() * (3 * sizeof(int16_t) + vkph::CHUNK_BYTE_SIZE);
    header.current_tick = state->current_tick;
    header.current_packet_count = ctx->current_packet;
    header.tag = ctx->tag;
    
    serialiser_t serialiser = {};
    serialiser.init(header.flags.total_packet_size);

    header.serialise(&serialiser);
    
    uint32_t chunks_in_packet = 0;

    uint8_t *chunk_count_byte = &serialiser.data_buffer[serialiser.data_buffer_head];

    // For now serialise 0
    serialiser.serialise_uint32(0);

    uint32_t chunk_values_start = serialiser.data_buffer_head;
    
    uint32_t index = clients_to_send_chunks_to.add();
    clients_to_send_chunks_to[index] = client->client_id;

    uint32_t total_chunks_to_send = 0;

    for (uint32_t i = 0; i < count; ++i) {
        if (s_serialise_chunk(&serialiser, &chunks_in_packet, values, i)) {
            ++total_chunks_to_send;
        }

        if (serialiser.data_buffer_head + 3 * sizeof(int16_t) + vkph::CHUNK_BYTE_SIZE > serialiser.data_buffer_size ||
            i + 1 == count) {
            // Need to send in new packet
            serialiser.serialise_uint32(chunks_in_packet, chunk_count_byte);

            // Serialise the header with the actual packet size
            uint32_t actual_packet_size = serialiser.data_buffer_head;
            header.flags.total_packet_size = actual_packet_size;
            serialiser.data_buffer_head = 0;
            header.serialise(&serialiser);
            serialiser.data_buffer_head = actual_packet_size;

            net::packet_chunk_voxels_t *packet_to_save = &client->chunk_packets[client->chunk_packet_count++];
            packet_to_save->chunk_data = FL_MALLOC(vkph::voxel_t, serialiser.data_buffer_head);
            memcpy(packet_to_save->chunk_data, serialiser.data_buffer, serialiser.data_buffer_head);
            //packet_to_save->chunk_data = serialiser.data_buffer;
            packet_to_save->size = serialiser.data_buffer_head;

            serialiser.data_buffer_head = chunk_values_start;

            LOG_INFOV("Packet contains %d chunks\n", chunks_in_packet);
            chunks_in_packet = 0;
        }
    }

    client->current_chunk_sending = 0;

    return total_chunks_to_send;
}

// Sends handshake, and starts sending voxels
static void s_send_game_state_to_new_client(
    uint16_t client_id,
    vkph::event_new_player_t *player_info,
    const vkph::state_t *state) {
    uint32_t loaded_chunk_count = 0;
    const vkph::chunk_t **chunks = state->get_active_chunks(&loaded_chunk_count);

    net::client_t *client = ctx->clients.get(client_id);

    // Send chunk information
    uint32_t max_chunks_per_packet = s_maximum_chunks_per_packet();
    LOG_INFOV("Maximum chunks per packet: %i\n", max_chunks_per_packet);

    net::voxel_chunk_values_t *voxel_chunks = LN_MALLOC(net::voxel_chunk_values_t, loaded_chunk_count);

    uint32_t count = 0;
    for (uint32_t i = 0; i < loaded_chunk_count; ++i) {
        const vkph::chunk_t *c = chunks[i];
        if (c) {
            voxel_chunks[count].x = c->chunk_coord.x;
            voxel_chunks[count].y = c->chunk_coord.y;
            voxel_chunks[count].z = c->chunk_coord.z;

            voxel_chunks[count].voxel_values = c->voxels;

            ++count;
        }
    }

    // Cannot send all of these at the same bloody time
    uint32_t chunks_to_send = s_prepare_packet_chunk_voxels(client, voxel_chunks, loaded_chunk_count, state);

    if (s_send_packet_connection_handshake(
        client_id,
        player_info,
        chunks_to_send,
        state)) {
        net::client_t *c = &ctx->clients[client_id];
        LOG_INFOV("Sent handshake to client: %s (with tag %d)\n", c->name, c->client_tag);
    }
    else {
        net::client_t *c = &ctx->clients[client_id];
        LOG_INFOV("Failed to send handshake to client: %s\n", c->name);
    }
}

// PT_PLAYER_JOINED
static void s_send_packet_player_joined(
    vkph::event_new_player_t *info,
    const vkph::state_t *state) {
    net::packet_player_joined_t packet = {};
    packet.player_info = info->info;

    vkph::player_flags_t flags = {};
    flags.u32 = packet.player_info.flags;
    flags.is_local = 0;

    packet.player_info.flags = flags.u32;

    net::packet_header_t header = {};
    header.current_tick = state->current_tick;
    header.current_packet_count = ctx->current_packet;
    header.flags.total_packet_size = header.size() + packet.size();
    header.flags.packet_type = net::PT_PLAYER_JOINED;
    header.tag = ctx->tag;
    
    serialiser_t serialiser = {};
    serialiser.init(header.flags.total_packet_size);
    
    header.serialise(&serialiser);
    packet.serialise(&serialiser);
    
    for (uint32_t i = 0; i < ctx->clients.data_count; ++i) {
        if (i != packet.player_info.client_id) {
            net::client_t *c = ctx->clients.get(i);
            if (c->initialised) {
                ctx->main_udp_send_to(&serialiser, c->address);
            }
        }
    }
}

// PT_CONNECTION_REQUEST
static net::client_t *s_receive_packet_connection_request(
    serialiser_t *serialiser,
    net::address_t address,
    const vkph::state_t *state,
    net::socket_t tcp_s) {
    net::packet_connection_request_t request = {};
    request.deserialise(serialiser);

    uint32_t client_id = ctx->clients.add();

    LOG_INFOV("New client with ID %i\n", client_id);

    net::client_t *client = ctx->clients.get(client_id);
    
    client->initialised = 1;
    client->client_id = client_id;
    client->name = create_fl_string(request.name);
    client->address = address;
    client->address.port = net::host_to_network_byte_order(request.used_port);

    LOG_INFOV("Now in communication with client at port %d\n", request.used_port);

    client->received_first_commands_packet = 0;
    client->predicted.chunk_mod_count = 0;
    client->predicted.chunk_modifications = (net::chunk_modifications_t *)ctx->chunk_modification_allocator.allocate_arena();
    client->previous_locations.init();
    client->tcp_socket = tcp_s;

    // Force a ping in the next loop
    client->ping = 0.0f;
    client->ping_in_progress = 0.0f;
    client->time_since_ping = net::NET_PING_INTERVAL + 0.1f;
    client->received_ping = 1;

    memset(client->predicted.chunk_modifications, 0, sizeof(net::chunk_modifications_t) * net::MAX_PREDICTED_CHUNK_MODIFICATIONS);
    
    vkph::event_new_player_t *event_data = FL_MALLOC(vkph::event_new_player_t, 1);
    event_data->info.client_name = client->name;
    event_data->info.client_id = client->client_id;
    // Need to calculate a random position
    // TODO: In future, make it so that there is like some sort of startup screen when joining a server (like choose team, etc..)
    event_data->info.ws_position = vector3_t(0.0f);
    event_data->info.ws_view_direction = vector3_t(1.0f, 0.0f, 0.0f);
    event_data->info.ws_up_vector = vector3_t(0.0f, 1.0f, 0.0f);
    event_data->info.default_speed = vkph::PLAYER_WALKING_SPEED;

    event_data->info.flags = 0;
    // Player starts of as dead
    // There are different ways of spawning the player: meteorite (basically uncontrollably flying into the world)
    // event_data->info.alive_state = 0;

    // Generate new position
    float x_rand = (float)(rand() % 100 + 100) * (rand() % 2 == 0 ? -1 : 1);
    float y_rand = (float)(rand() % 100 + 100) * (rand() % 2 == 0 ? -1 : 1);
    float z_rand = (float)(rand() % 100 + 100) * (rand() % 2 == 0 ? -1 : 1);
    event_data->info.next_random_spawn_position = vector3_t(x_rand, y_rand, z_rand);
    
    submit_event(vkph::ET_NEW_PLAYER, event_data);

    // Send game state to new player
    s_send_game_state_to_new_client(client_id, event_data, state);

    // Dispatch to all players newly joined player information
    s_send_packet_player_joined(event_data, state);

    return client;
}

static void s_handle_disconnect(
    uint16_t client_id,
    const vkph::state_t *state) {
    LOG_INFO("Client disconnected\n");

    client_tag_to_id.remove(ctx->clients[client_id].client_tag);

    ctx->clients[client_id].predicted.chunk_modifications;
    ctx->chunk_modification_allocator.free_arena(ctx->clients[client_id].predicted.chunk_modifications);
    ctx->clients[client_id].previous_locations.destroy();
    ctx->clients[client_id].initialised = 0;
    ctx->clients.remove(client_id);

    vkph::event_player_disconnected_t *data = FL_MALLOC(vkph::event_player_disconnected_t, 1);
    data->client_id = client_id;
    vkph::submit_event(vkph::ET_PLAYER_DISCONNECTED, data);

    serialiser_t out_serialiser = {};
    out_serialiser.init(100);
    net::packet_header_t header = {};
    header.current_tick = state->current_tick;
    header.current_packet_count = ctx->current_packet;
    header.flags.packet_type = net::PT_PLAYER_LEFT;
    header.flags.total_packet_size = header.size() + sizeof(uint16_t);
    header.tag = ctx->tag;

    header.serialise(&out_serialiser);
    out_serialiser.serialise_uint16(client_id);
    
    for (uint32_t i = 0; i < ctx->clients.data_count; ++i) {
        net::client_t *c = &ctx->clients[i];
        if (c->initialised) {
            ctx->main_udp_send_to(&out_serialiser, c->address);
        }
    }
}

// PT_CLIENT_DISCONNECT
static void s_receive_packet_client_disconnect(
    uint16_t client_id,
    const vkph::state_t *state) {
    s_handle_disconnect(client_id, state);
}

static void s_handle_chunk_modifications(
    net::packet_client_commands_t *commands,
    net::client_t *client,
    vkph::state_t *state) {
    ctx->merge_chunk_modifications(
        client->predicted.chunk_modifications, &client->predicted.chunk_mod_count,
        commands->prediction.chunk_modifications, commands->prediction.chunk_mod_count,
        state);
}

// PT_CLIENT_COMMANDS
static void s_receive_packet_client_commands(
    serialiser_t *serialiser,
    uint16_t client_id,
    uint64_t tick,
    vkph::state_t *state) {
    int32_t local_id = state->get_local_id(client_id);
    vkph::player_t *p = state->get_player(local_id);

    if (p) {
        net::client_t *c = &ctx->clients[p->client_id];

        c->received_first_commands_packet = 1;

        net::packet_client_commands_t commands = {};
        commands.deserialise(serialiser);

        if (commands.requested_spawn) {
            spawn_player(client_id, state);
        }
        
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

                //LOG_INFOV("Received Tick %llu\n", (unsigned long long)c->tick);
            }

            if (commands.command_count) {
                LOG_NETWORK_DEBUG("--- Received commands from client\n");
            }

            for (uint32_t i = 0; i < commands.command_count; ++i) {
                //LOG_INFOV("Accumulated dt: %f\n", commands.actions[i].accumulated_dt);
                p->push_actions(&commands.actions[i], 1);

                LOG_NETWORK_DEBUGV("Tick %lu; actions %d; dmouse_x %f; dmouse_y %f; dt %f\n",
                                   commands.actions[i].tick,
                                   commands.actions[i].bytes,
                                   commands.actions[i].dmouse_x,
                                   commands.actions[i].dmouse_y,
                                   commands.actions[i].dt);
            }

            if (commands.command_count) {
                LOG_NETWORK_DEBUGV("Final velocity %f %f %f\n\n", commands.ws_final_velocity.x, commands.ws_final_velocity.y, commands.ws_final_velocity.z);
            }

            c->predicted.ws_position = commands.prediction.ws_position;
            c->predicted.ws_view_direction = commands.prediction.ws_view_direction;
            c->predicted.ws_up_vector = commands.prediction.ws_up_vector;
            c->predicted.ws_velocity = commands.prediction.ws_velocity;
            c->predicted.player_flags.u32 = commands.prediction.player_flags.u32;
            c->predicted.player_health = commands.prediction.player_health;

            // Predicted projectiles
#if 0
            for (uint32_t i = 0; i < commands.predicted_hit_count; ++i) {
                c->predicted_proj_hits[c->predicted_proj_hit_count + i] = commands.hits[i];
            }

            c->predicted_proj_hit_count += commands.predicted_hit_count;
#endif

            if (!c->predicted.player_flags.is_alive) {
                // LOG_INFO("Player is now dead!\n");
            }
            
            // Process terraforming stuff
            if (commands.prediction.chunk_mod_count) {
                //LOG_INFOV("(Tick %llu) Received %i chunk modifications\n", (unsigned long long)tick, commands.modified_chunk_count);
                c->did_terrain_mod_previous_tick = 1;
                c->tick_at_which_client_terraformed = tick;

                if (commands.prediction.chunk_mod_count) {
#if 1 // This is for debugging - should be removed later on...
                    // printf("\n");
                    // LOG_INFOV("Predicted %i chunk modifications at tick %llu\n", commands.modified_chunk_count, (unsigned long long)tick);
                    for (uint32_t i = 0; i < commands.prediction.chunk_mod_count; ++i) {
                        //LOG_INFOV("In chunk (%i %i %i): \n", commands.chunk_modifications[i].x, commands.chunk_modifications[i].y, commands.chunk_modifications[i].z);
                        const vkph::chunk_t *c_ptr = state->get_chunk(
                            ivector3_t(
                                commands.prediction.chunk_modifications[i].x,
                                commands.prediction.chunk_modifications[i].y,
                                commands.prediction.chunk_modifications[i].z));

                        for (uint32_t v = 0; v < commands.prediction.chunk_modifications[i].modified_voxels_count; ++v) {
                            uint8_t initial_value = c_ptr->voxels[commands.prediction.chunk_modifications[i].modifications[v].index].value;
                            if (c_ptr->history.modification_pool[commands.prediction.chunk_modifications[i].modifications[v].index] != vkph::CHUNK_SPECIAL_VALUE) {
                                //initial_value = c_ptr->history.modification_pool[commands.chunk_modifications[i].modifications[v].index];
                            }
                            if (initial_value != commands.prediction.chunk_modifications[i].modifications[v].initial_value) {
                                LOG_INFOV(
                                    "(Voxel %i) INITIAL VALUES ARE NOT THE SAME: %i != %i\n",
                                    commands.prediction.chunk_modifications[i].modifications[v].index,
                                    (int32_t)initial_value,
                                    (int32_t)commands.prediction.chunk_modifications[i].modifications[v].initial_value);
                            }
                            //LOG_INFOV("- index %i | initial value %i | final value %i\n", (int32_t)commands.chunk_modifications[i].modifications[v].index, initial_value, (int32_t)commands.chunk_modifications[i].modifications[v].final_value);
                        }
                    }
#endif
                }
                
                s_handle_chunk_modifications(&commands, c, state);
            }
        }
    }
    else {
        // There is a problem
        LOG_ERROR("Player was not initialised yet, cannot process client commands!\n");
    }
}

static void s_receive_packet_team_select_request(
    serialiser_t *serialiser,
    uint16_t client_id,
    uint64_t tick,
    vkph::state_t *state) {
    auto color = (vkph::team_color_t)serialiser->deserialise_uint32();

    if (state->check_team_joinable(color)) {
        int32_t local_id = state->get_local_id(client_id);
        vkph::player_t *p = state->get_player(local_id);

        LOG_INFOV("Player %s just joined team %s\n", p->name, team_color_to_string(color));

        // Able to join team
        state->change_player_team(p, color);

        // If the player is alive
        if (p->flags.is_alive) {
            p->flags.is_alive = false;
        }

        p->terraform_package.color = team_color_to_voxel_color(color);

        serialiser_t out_serialiser = {};
        out_serialiser.init(20);

        net::packet_player_team_change_t change = {};
        change.client_id = client_id;
        change.color = (uint16_t)color;

        net::packet_header_t header = {};
        header.current_tick = state->current_tick;
        header.current_packet_count = ctx->current_packet;
        header.flags.packet_type = net::PT_PLAYER_TEAM_CHANGE;
        header.flags.total_packet_size = header.size() + change.size();
        header.tag = ctx->tag;

        header.serialise(&out_serialiser);
        change.serialise(&out_serialiser);

        // Send to all players
        for (uint32_t i = 0; i < ctx->clients.data_count; ++i) {
            ctx->main_udp_send_to(&out_serialiser, ctx->clients.get(i)->address);
        }
    }
    else {
        // Unable to join team
        // Mark player as having made an error
        // TODO
    }
}

static bool s_check_if_client_has_to_correct_state(vkph::player_t *p, net::client_t *c) {
    vector3_t dposition = glm::abs(p->ws_position - c->predicted.ws_position);
    vector3_t ddirection = glm::abs(p->ws_view_direction - c->predicted.ws_view_direction);
    vector3_t dup = glm::abs(p->ws_up_vector - c->predicted.ws_up_vector);
    vector3_t dvelocity = glm::abs(p->ws_velocity - c->predicted.ws_velocity);

    float precision = 0.000001f;
    bool incorrect_position = 0;
    if (dposition.x >= precision || dposition.y >= precision || dposition.z >= precision) {
        LOG_INFOV(
            "Need to correct position: %f %f %f <- %f %f %f\n",
            p->ws_position.x,
            p->ws_position.y,
            p->ws_position.z,
            c->predicted.ws_position.x,
            c->predicted.ws_position.y,
            c->predicted.ws_position.z);

        incorrect_position = 1;
    }

    bool incorrect_direction = 0;
    if (ddirection.x >= precision || ddirection.y >= precision || ddirection.z >= precision) {
        LOG_INFOV(
            "Need to correct position: %f %f %f <- %f %f %f\n",
            p->ws_view_direction.x,
            p->ws_view_direction.y,
            p->ws_view_direction.z,
            c->predicted.ws_view_direction.x,
            c->predicted.ws_view_direction.y,
            c->predicted.ws_view_direction.z);

        incorrect_direction = 1;
    }

    bool incorrect_up = 0;
    if (dup.x >= precision || dup.y >= precision || dup.z >= precision) {
        LOG_INFOV(
            "Need to correct up vector: %f %f %f <- %f %f %f\n",
            p->ws_up_vector.x,
            p->ws_up_vector.y,
            p->ws_up_vector.z,
            c->predicted.ws_up_vector.x,
            c->predicted.ws_up_vector.y,
            c->predicted.ws_up_vector.z);

        incorrect_up = 1;
    }

    bool incorrect_velocity = 0;
    if (dvelocity.x >= precision || dvelocity.y >= precision || dvelocity.z >= precision) {
        incorrect_velocity = 1;
        LOG_INFOV(
            "Need to correct velocity: %f %f %f <- %f %f %f\n",
            p->ws_velocity.x,
            p->ws_velocity.y,
            p->ws_velocity.z,
            c->predicted.ws_velocity.x,
            c->predicted.ws_velocity.y,
            c->predicted.ws_velocity.z);
    }

    bool incorrect_interaction_mode = 0;
    if (p->flags.interaction_mode != c->predicted.player_flags.interaction_mode) {
        LOG_INFOV(
            "Need to correct interaction mode: %d to %d\n",
            (uint32_t)c->predicted.player_flags.interaction_mode,
            (uint32_t)p->flags.interaction_mode);

        incorrect_interaction_mode = 1;
    }

    bool incorrect_alive_state = 0;
    if (p->flags.is_alive != c->predicted.player_flags.is_alive) {
        LOG_INFO("Need to correct alive state\n");
        incorrect_alive_state = 1;
    }

    return incorrect_position ||
        incorrect_direction ||
        incorrect_up ||
        incorrect_velocity ||
        incorrect_interaction_mode ||
        incorrect_alive_state;
}

static bool s_check_if_client_has_to_correct_terrain(net::client_t *c, vkph::state_t *state) {
    bool needs_to_correct = 0;

    for (uint32_t cm_index = 0; cm_index < c->predicted.chunk_mod_count; ++cm_index) {
        net::chunk_modifications_t *cm_ptr = &c->predicted.chunk_modifications[cm_index];
        cm_ptr->needs_to_correct = 0;

        auto *c_ptr = state->get_chunk(ivector3_t(cm_ptr->x, cm_ptr->y, cm_ptr->z));

        bool chunk_has_mistake = 0;
        
        for (uint32_t vm_index = 0; vm_index < cm_ptr->modified_voxels_count; ++vm_index) {
            net::voxel_modification_t *vm_ptr = &cm_ptr->modifications[vm_index];

            vkph::voxel_t *voxel_ptr = &c_ptr->voxels[vm_ptr->index];
            uint8_t actual_value = voxel_ptr->value;
            uint8_t predicted_value = vm_ptr->final_value;

            vkph::voxel_color_t color = voxel_ptr->color;

            // Just one mistake can completely mess stuff up between the client and server
            if (actual_value != predicted_value) {
#if 0
                printf("(%i %i %i) Need to set (%i) %i -> %i\n", c_ptr->chunk_coord.x, c_ptr->chunk_coord.y, c_ptr->chunk_coord.z, vm_ptr->index, (int32_t)predicted_value, (int32_t)actual_value);
#endif

                // Change the predicted value and send this back to the client, and send this back to the client to correct
                vm_ptr->final_value = actual_value;
                vm_ptr->color = color;

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
    net::packet_game_state_snapshot_t *snapshot,
    vkph::state_t *state) {
    // Up to 300 chunks can be modified between game dispatches
    net::chunk_modifications_t *modifications = LN_MALLOC(net::chunk_modifications_t, net::NET_MAX_ACCUMULATED_PREDICTED_CHUNK_MODIFICATIONS_PER_PACK);

    // Don't need the initial values - the client will use its values for voxels as the "initial" values
    uint32_t modification_count = fill_chunk_modification_array_with_colors(modifications, state);

    snapshot->modified_chunk_count = modification_count;
    snapshot->chunk_modifications = modifications;
}

static void s_add_projectiles_to_game_state_snapshot(
    net::packet_game_state_snapshot_t *snapshot,
    vkph::state_t *state) {
    snapshot->rock_count = state->rocks.recent_count;
    snapshot->rock_snapshots = LN_MALLOC(vkph::rock_snapshot_t, snapshot->rock_count);

    for (uint32_t i = 0; i < snapshot->rock_count; ++i) {
        uint32_t new_rock_idx = state->rocks.recent[i];
        vkph::rock_t *rock = state->rocks.list.get(new_rock_idx);

        snapshot->rock_snapshots[i].client_id = rock->client_id;
        snapshot->rock_snapshots[i].position = rock->position;
        snapshot->rock_snapshots[i].direction = rock->direction;
        snapshot->rock_snapshots[i].up = rock->up;
    }
}

// PT_GAME_STATE_SNAPSHOT
static void s_send_packet_game_state_snapshot(vkph::state_t *state) {
#if NET_DEBUG || NET_DEBUG_VOXEL_INTERPOLATION
    printf("\n\n GAME STATE DISPATCH\n");
#endif
    
    net::packet_game_state_snapshot_t packet = {};
    packet.player_data_count = 0;
    packet.player_snapshots = LN_MALLOC(vkph::player_snapshot_t, ctx->clients.data_count);

    s_add_chunk_modifications_to_game_state_snapshot(&packet, state);

    s_add_projectiles_to_game_state_snapshot(&packet, state);
    // Need to clear the "newly spawned rocks" stack
    state->rocks.clear_recent();
    
    for (uint32_t i = 0; i < ctx->clients.data_count; ++i) {
        net::client_t *c = &ctx->clients[i];

        if (c->initialised && c->received_first_commands_packet) {
            c->should_set_tick = 1;

            // Check if the data that the client predicted was correct, if not, force client to correct position
            // Until server is sure that the client has done a correction, server will not process this client's commands
            vkph::player_snapshot_t *snapshot = &packet.player_snapshots[packet.player_data_count];
            snapshot->flags = 0;

            int32_t local_id = state->get_local_id(c->client_id);
            vkph::player_t *p = state->get_player(local_id);

            // Check if player has to correct general state (position, view direction, etc...)
            bool has_to_correct_state = s_check_if_client_has_to_correct_state(p, c);
            // Check if client has to correct voxel modifications
            bool has_to_correct_terrain = s_check_if_client_has_to_correct_terrain(c, state);

            if (has_to_correct_state || has_to_correct_terrain) {
                if (c->waiting_on_correction) {
                    // TODO: Make sure to relook at this, so that in case of packet loss, the server doesn't just stall at this forever
                    LOG_INFOV("(%llu) Client needs to do correction, but did not receive correction acknowledgement, not sending correction\n", (long long unsigned int)state->current_tick);
                    snapshot->client_needs_to_correct_state = 0;
                    snapshot->server_waiting_for_correction = 1;
                }
                else {
                    // If there is a correction of any kind to do, force client to correct everything
                    c->waiting_on_correction = 1;

                    if (has_to_correct_terrain) {
                        snapshot->packet_contains_terrain_correction = 1;
                        c->send_corrected_predicted_voxels = 1;
                    }

                    LOG_INFOV("Client needs to revert to tick %llu\n\n", (unsigned long long)c->tick);
                    snapshot->client_needs_to_correct_state = has_to_correct_state || has_to_correct_terrain;
                    snapshot->server_waiting_for_correction = 0;
                    p->cached_player_action_count = 0;
                    p->player_action_count = 0;
                }
            }
            else {
                snapshot->client_needs_to_correct_state = 0;
                snapshot->server_waiting_for_correction = 0;
            }

            snapshot->client_id = c->client_id;
            snapshot->player_local_flags = p->flags.u32;
            snapshot->player_health = p->health;
            snapshot->ws_position = p->ws_position;
            snapshot->ws_view_direction = p->ws_view_direction;
            snapshot->ws_next_random_spawn = p->next_random_spawn_position;
            snapshot->ws_velocity = p->ws_velocity;
            snapshot->ws_up_vector = p->ws_up_vector;
            snapshot->tick = c->tick;
            snapshot->terraformed = c->did_terrain_mod_previous_tick;
            snapshot->interaction_mode = p->flags.interaction_mode;
            snapshot->alive_state = p->flags.is_alive;
            snapshot->contact = p->flags.is_on_ground;
            snapshot->animated_state = p->animated_state;
            snapshot->frame_displacement = p->frame_displacement;

            // Add snapshot to client's circular buffer of snapshots
            vkph::player_position_snapshot_t s = {};
            s.ws_position = snapshot->ws_position;
            s.tick = snapshot->tick;
            c->previous_locations.push_item(&s);
            
            if (snapshot->terraformed) {
                snapshot->terraform_tick = c->tick_at_which_client_terraformed;
            }

            // Reset
            c->did_terrain_mod_previous_tick = 0;
            c->tick_at_which_client_terraformed = 0;

            ++packet.player_data_count;
        }
    }

#if 0
    LOG_INFOV("Modified %i chunks\n", packet.modified_chunk_count);
    for (uint32_t i = 0; i < packet.modified_chunk_count; ++i) {
        LOG_INFOV("In chunk (%i %i %i): \n", packet.chunk_modifications[i].x, packet.chunk_modifications[i].y, packet.chunk_modifications[i].z);
        for (uint32_t v = 0; v < packet.chunk_modifications[i].modified_voxels_count; ++v) {
            LOG_INFOV("- index %i | initial value %i | final value %i\n", (int32_t)packet.chunk_modifications[i].modifications[v].index, (int32_t)packet.chunk_modifications[i].modifications[v].initial_value, (int32_t)packet.chunk_modifications[i].modifications[v].final_value);
        }
    }
#endif

    net::packet_header_t header = {};
    header.current_tick = state->current_tick;
    header.current_packet_count = ctx->current_packet;
    // Don't need to fill this
    header.client_id = 0;
    header.flags.packet_type = net::PT_GAME_STATE_SNAPSHOT;
    header.flags.total_packet_size = header.size() + packet.size();
    header.tag = ctx->tag;

    serialiser_t serialiser = {};
    serialiser.init(header.flags.total_packet_size);

    header.serialise(&serialiser);

    // This is the packet for players that need correction
    packet.serialise(&serialiser);
    // In here, need to serialise chunk modifications with the union for colors, instead of serialising the separate, color array
    net::serialise_chunk_modifications(
        packet.chunk_modifications,
        packet.modified_chunk_count,
        &serialiser,
        net::CST_SERIALISE_UNION_COLOR);
    
    uint32_t data_head_before = serialiser.data_buffer_head;
    
    for (uint32_t i = 0; i < ctx->clients.data_count; ++i) {
        net::client_t *c = &ctx->clients[i];

        if (c->send_corrected_predicted_voxels) {
            // Serialise
            LOG_INFOV("Need to correct %i chunks\n", c->predicted.chunk_mod_count);
            net::serialise_chunk_modifications(
                c->predicted.chunk_modifications,
                c->predicted.chunk_mod_count,
                &serialiser,
                net::CST_SERIALISE_UNION_COLOR);
        }
        
        if (c->initialised && c->received_first_commands_packet) {
            ctx->main_udp_send_to(&serialiser, c->address);
        }

        serialiser.data_buffer_head = data_head_before;
        
        // Clear client's predicted modification array
        c->predicted.chunk_mod_count = 0;
        c->send_corrected_predicted_voxels = 0;
    }

    state->reset_modification_tracker();
}

// PT_CHUNK_VOXELS
static void s_send_pending_chunks() {
    uint32_t to_remove_count = 0;
    uint32_t *to_remove = LN_MALLOC(uint32_t, clients_to_send_chunks_to.data_count);
    for (uint32_t i = 0; i < clients_to_send_chunks_to.data_count; ++i) {
        uint32_t client_id = clients_to_send_chunks_to[i];
        net::client_t *c_ptr = &ctx->clients[client_id];

        if (c_ptr->current_chunk_sending < c_ptr->chunk_packet_count) {
            net::packet_chunk_voxels_t *packet = &c_ptr->chunk_packets[c_ptr->current_chunk_sending];
            serialiser_t serialiser = {};
            serialiser.data_buffer_head = packet->size;
            serialiser.data_buffer = (uint8_t *)packet->chunk_data;
            serialiser.data_buffer_size = packet->size;

            if (net::send_to_bound_address(c_ptr->tcp_socket, (char *)serialiser.data_buffer, serialiser.data_buffer_head)) {
                LOG_INFOV("Sent chunk packet of size %d\n", serialiser.data_buffer_head);
            }

            // Free
            FL_FREE(packet->chunk_data);

            c_ptr->current_chunk_sending++;
        }
        else {
            to_remove[to_remove_count++] = i;
        }
    }

    for (uint32_t i = 0; i < to_remove_count; ++i) {
        clients_to_send_chunks_to.remove(to_remove[i]);
    }
}

static void s_ping_clients(const vkph::state_t *state) {
    // Send a ping
    serialiser_t serialiser = {};
    serialiser.init(30);

    for (uint32_t i = 0; i < ctx->clients.data_count; ++i) {
        net::client_t* c = &ctx->clients[i];

        if (c->initialised) {
            c->time_since_ping += delta_time();
            c->ping_in_progress += delta_time();

            if (c->time_since_ping > net::NET_CLIENT_TIMEOUT) {
                // LOG_INFOV("Client %d (%s) timeout\n", c->client_id, c->name);

#if !defined(DEBUGGING)
                s_handle_disconnect(c->client_id, state);
#endif
            }
            else if (c->time_since_ping > net::NET_PING_INTERVAL && c->received_ping) {
                serialiser.data_buffer_head = 0;

                net::packet_header_t header = {};
                header.current_packet_count = ctx->current_packet;
                header.current_tick = state->current_tick;
                header.client_id = c->client_id;
                header.flags.packet_type = net::PT_PING;
                header.flags.total_packet_size = header.size();
                header.tag = ctx->tag;

                header.serialise(&serialiser);
                ctx->main_udp_send_to(&serialiser, c->address);

                c->time_since_ping = 0.0f;
                c->ping_in_progress = 0.0f;

                c->received_ping = 0;
                serialiser.data_buffer_head = 0;
            }
        }
    }
}

static void s_receive_packet_ping(
    serialiser_t *serialiser,
    uint16_t client_id,
    uint64_t current_tick) {
    net::client_t *c = &ctx->clients[client_id];

    c->received_ping = 1;
    c->ping = c->ping_in_progress;
    c->ping_in_progress = 0.0f;
}

static void s_check_pending_connections(vkph::state_t *state) {
    net::accepted_connection_t conn = net::accept_connection(ctx->main_tcp_socket);

    if (conn.s >= 0) {
        // Accept was successful
        LOG_INFO("New connection! Waiting for connection request packet...\n");

        uint32_t idx = pending_conns.add();
        auto *pconn = &pending_conns[idx];
        pconn->elapsed_time = 0.0f;
        pconn->pending = 1;
        pconn->s = conn.s;
        pconn->addr = conn.address;

        net::set_socket_blocking_state(pconn->s, 0);
    }
    else {
        // Accept wasn't sucessful (non-blocking).
    }

    for (uint32_t i = 0; i < pending_conns.data_count; ++i) {
        auto *pconn = &pending_conns[i];

        if (pconn->pending) {
            int32_t byte_count = net::receive_from_bound_address(pconn->s, ctx->message_buffer, sizeof(char) * net::NET_MAX_MESSAGE_SIZE);

            if (byte_count > 0) {
                serialiser_t in_serialiser = {};
                in_serialiser.data_buffer = (uint8_t *)ctx->message_buffer;
                in_serialiser.data_buffer_size = byte_count;

                net::packet_header_t header = {};
                header.deserialise(&in_serialiser);

                if (header.flags.packet_type == net::PT_CONNECTION_REQUEST) {
                    if (header.tag == net::UNINITIALISED_TAG) {
                        net::address_t addr = pconn->addr;
                        // addr.port = net::host_to_network_byte_order(net::GAME_OUTPUT_PORT_CLIENT);;

                        net::client_t *new_client = s_receive_packet_connection_request(&in_serialiser, addr, state, pconn->s);

                        net::set_socket_send_buffer_size(new_client->tcp_socket, net::NET_MAX_MESSAGE_SIZE * 2);

                        pconn->pending = 0;
                        pending_conns.remove(i);

                        LOG_INFOV("Removed pending connection (%d - initialised or not - are left in the array)\n", pending_conns.data_count);
                    }
                }
            }
            else {
#if 0

                pconn->elapsed_time += state->delta_time;

                if (pconn->elapsed_time > 5.0f) {
                    LOG_INFO("Pending connection timed out\n");
                    
                    pconn->pending = 0;
                    net::destroy_socket(pconn->s);
                    
                    pending_conns.remove(i);
                    
                    LOG_INFOV("Removed pending connection (%d)\n", pending_conns.data_count);
                }
#endif
            }
        }
    }
}

static void s_tick_server(vkph::state_t *state) {
    s_check_pending_connections(state);
    s_ping_clients(state);

    static float snapshot_elapsed = 0.0f;
    snapshot_elapsed += delta_time();
    
    if (snapshot_elapsed >= net::NET_SERVER_SNAPSHOT_OUTPUT_INTERVAL) {
        // Send commands to the server
        s_send_packet_game_state_snapshot(state);

        snapshot_elapsed = 0.0f;
    }

    // For sending chunks to new players
    static float world_elapsed = 0.0f;
    world_elapsed += delta_time();

    if (world_elapsed >= net::NET_SERVER_CHUNK_WORLD_OUTPUT_INTERVAL) {
        s_send_pending_chunks();

        world_elapsed = 0.0f;
    }

    for (uint32_t i = 0; i < ctx->clients.data_count + 1; ++i) {
        net::address_t received_address = {};
        int32_t received = ctx->main_udp_recv_from(
            ctx->message_buffer,
            sizeof(char) * net::NET_MAX_MESSAGE_SIZE,
            &received_address);

        if (received > 0) {
            serialiser_t in_serialiser = {};
            in_serialiser.data_buffer = (uint8_t *)ctx->message_buffer;
            in_serialiser.data_buffer_size = received;

            net::packet_header_t header = {};
            header.deserialise(&in_serialiser);

            uint16_t *id_p = client_tag_to_id.get(header.tag);

            if (id_p) {
                auto *client = &ctx->clients[*id_p];
                client->address = received_address;

                switch (header.flags.packet_type) {
                    
                case net::PT_CLIENT_DISCONNECT: {
                    s_receive_packet_client_disconnect(header.client_id, state);
                } break;

                case net::PT_CLIENT_COMMANDS: {
                    s_receive_packet_client_commands(&in_serialiser, header.client_id, header.current_tick, state);
                } break;

                case net::PT_TEAM_SELECT_REQUEST: {
                    s_receive_packet_team_select_request(&in_serialiser, header.client_id, header.current_tick, state);
                } break;

                case net::PT_PING: {
                    s_receive_packet_ping(&in_serialiser, header.client_id, header.current_tick);
                } break;

                }
            }
            else {
                LOG_INFO("Received packet from unidentified client\n");
            }
        }
    }
}

static vkph::listener_t net_listener_id;

static void s_net_event_listener(void *object, vkph::event_t *event) {
    vkph::state_t *state = (vkph::state_t *)object;

    switch (event->type) {

    case vkph::ET_START_SERVER: {
        vkph::event_start_server_t *data = (vkph::event_start_server_t *)event->data;
        s_start_server(data, state);

        FL_FREE(data);
    } break;

    default: {
    } break;

    }
}

void init_net(vkph::state_t *state) {
    ctx = flmalloc<net::context_t>();

    net_listener_id = vkph::set_listener_callback(&s_net_event_listener, state);
    vkph::subscribe_to_event(vkph::ET_START_SERVER, net_listener_id);

    net::init_socket_api();

    ctx->message_buffer = FL_MALLOC(char, net::NET_MAX_MESSAGE_SIZE);

    // meta_socket_init();
    init_meta_connection();
    check_registration();
}

void tick_net(vkph::state_t *state) {
    s_tick_server(state);
}

const net::client_t *get_client(uint32_t i) {
    return &ctx->clients[i];
}


}
