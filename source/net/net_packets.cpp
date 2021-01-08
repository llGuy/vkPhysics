#include "net_packets.hpp"
#include "net_socket.hpp"
#include "vkph_player.hpp"
#include "net_context.hpp"

namespace net {

const char *packet_type_to_str(packet_type_t type) {
    switch (type) {
    case PT_CONNECTION_REQUEST: return "CONNECTION_REQUEST";
    case PT_PING: return "PING";
    case PT_CONNECTION_HANDSHAKE: return "CONNECTION_HANDSHAKE";
    case PT_PLAYER_JOINED: return "PLAYER_JOINED";
    case PT_TEAM_SELECT_REQUEST: return "TEAM_SELECT_REQUEST";
    case PT_PLAYER_TEAM_CHANGE: return "PLAYER_TEAM_CHANGE";
    case PT_CLIENT_DISCONNECT: return "CLIENT_DISCONNECT";
    case PT_PLAYER_LEFT: return "PLAYER_LEFT";
    case PT_CLIENT_COMMANDS: return "CLIENT_COMMANDS";
    case PT_GAME_STATE_SNAPSHOT: return "GAME_STATE_SNAPSHOT";
    case PT_CHUNK_VOXELS: return "CHUNK_VOXELS";
    default: return "INVALID";
    }
}

uint32_t packet_header_t::size() {
    return
        sizeof(flags) +
        sizeof(current_tick) +
        sizeof(current_packet_count) +
        sizeof(tag) +
        sizeof(client_id);
}

void packet_header_t::serialise(serialiser_t *serialiser) {
    serialiser->serialise_uint32(flags.bytes);
    serialiser->serialise_uint64(current_tick);
    serialiser->serialise_uint64(current_packet_count);
    serialiser->serialise_uint32(tag);
    serialiser->serialise_uint16(client_id);
}

void packet_header_t::deserialise(serialiser_t *serialiser) {
    flags.bytes = serialiser->deserialise_uint32();
    current_tick = serialiser->deserialise_uint64();
    current_packet_count = serialiser->deserialise_uint64();
    tag = serialiser->deserialise_uint32();
    client_id = serialiser->deserialise_uint16();
}

uint32_t packet_connection_request_t::size() {
    return sizeof(uint8_t) * (uint32_t)strlen(name);
}

void packet_connection_request_t::serialise(serialiser_t *serialiser) {
    serialiser->serialise_string(name);
}

void packet_connection_request_t::deserialise(serialiser_t *serialiser) {
    name = serialiser->deserialise_string();
}

uint32_t packet_connection_handshake_t::size() {
    uint32_t final_size = 0;
    final_size += sizeof(client_tag);
    final_size += sizeof(loaded_chunk_count);
    final_size += sizeof(player_count);
    final_size += player_count * sizeof(vkph::player_init_info_t);
    final_size += sizeof(team_count);
    final_size += team_count * sizeof(vkph::team_info_t);

    // Other information...
    return final_size;
}

void packet_connection_handshake_t::serialise(serialiser_t *serialiser) {
    serialiser->serialise_uint32(client_tag);
    serialiser->serialise_uint32(loaded_chunk_count);
    serialiser->serialise_uint32(player_count);
    for (uint32_t i = 0; i < player_count; ++i) {
        serialiser->serialise_string(player_infos[i].client_name);
        serialiser->serialise_uint16(player_infos[i].client_id);
        serialiser->serialise_vector3(player_infos[i].ws_position);
        serialiser->serialise_vector3(player_infos[i].ws_view_direction);
        serialiser->serialise_vector3(player_infos[i].ws_up_vector);
        serialiser->serialise_vector3(player_infos[i].next_random_spawn_position);
        serialiser->serialise_float32(player_infos[i].default_speed);
        serialiser->serialise_uint32(player_infos[i].flags);
    }

    serialiser->serialise_uint32(team_count);
    for (uint32_t i = 0; i < team_count; ++i) {
        serialiser->serialise_uint32(team_infos[i].color);
        serialiser->serialise_uint32(team_infos[i].player_count);
        serialiser->serialise_uint32(team_infos[i].max_players);
    }
}

void packet_connection_handshake_t::deserialise(serialiser_t *serialiser) {
    client_tag = serialiser->deserialise_uint32();
    loaded_chunk_count = serialiser->deserialise_uint32();
    player_count = serialiser->deserialise_uint32();
    player_infos = LN_MALLOC(vkph::player_init_info_t, player_count);

    for (uint32_t i = 0; i < player_count; ++i) {
        player_infos[i].client_name = serialiser->deserialise_string();
        player_infos[i].client_id = serialiser->deserialise_uint16();
        player_infos[i].ws_position = serialiser->deserialise_vector3();
        player_infos[i].ws_view_direction = serialiser->deserialise_vector3();
        player_infos[i].ws_up_vector = serialiser->deserialise_vector3();
        player_infos[i].next_random_spawn_position = serialiser->deserialise_vector3();
        player_infos[i].default_speed = serialiser->deserialise_float32();
        player_infos[i].flags = serialiser->deserialise_uint32();
    }

    team_count = serialiser->deserialise_uint32();
    team_infos = LN_MALLOC(vkph::team_info_t, team_count);
    for (uint32_t i = 0; i < team_count; ++i) {
        team_infos[i].color = (vkph::team_color_t)serialiser->deserialise_uint32();
        team_infos[i].player_count = serialiser->deserialise_uint32();
        team_infos[i].max_players = serialiser->deserialise_uint32();
    }
}

uint32_t packet_player_joined_t::size() {
    uint32_t total_size = 0;
    total_size += (uint32_t)strlen(player_info.client_name);
    total_size += sizeof(vkph::player_init_info_t::client_id);
    total_size += sizeof(vkph::player_init_info_t::ws_position);
    total_size += sizeof(vkph::player_init_info_t::ws_view_direction);
    total_size += sizeof(vkph::player_init_info_t::ws_up_vector);
    total_size += sizeof(vkph::player_init_info_t::default_speed);
    total_size += sizeof(vkph::player_init_info_t::flags);

    return total_size;
}

void packet_player_joined_t::serialise(serialiser_t *serialiser) {
    serialiser->serialise_string(player_info.client_name);
    serialiser->serialise_uint16(player_info.client_id);
    serialiser->serialise_vector3(player_info.ws_position);
    serialiser->serialise_vector3(player_info.ws_view_direction);
    serialiser->serialise_vector3(player_info.ws_up_vector);
    serialiser->serialise_float32(player_info.default_speed);
    serialiser->serialise_uint8(player_info.flags);
}

void packet_player_joined_t::deserialise(serialiser_t *serialiser) {
    player_info.client_name = serialiser->deserialise_string();
    player_info.client_id = serialiser->deserialise_uint16();
    player_info.ws_position = serialiser->deserialise_vector3();
    player_info.ws_view_direction = serialiser->deserialise_vector3();
    player_info.ws_up_vector = serialiser->deserialise_vector3();
    player_info.default_speed = serialiser->deserialise_float32();
    player_info.flags = serialiser->deserialise_uint32();
}

uint32_t packet_client_commands_t::size() {
    uint32_t final_size = 0;
    final_size += sizeof(command_count);

    uint32_t command_size =
        sizeof(vkph::player_action_t::bytes) +
        sizeof(vkph::player_action_t::dmouse_x) +
        sizeof(vkph::player_action_t::dmouse_y) +
        sizeof(vkph::player_action_t::dt) +
        sizeof(vkph::player_action_t::accumulated_dt);

    final_size += command_size * command_count;

    final_size += sizeof(prediction.player_flags);
    final_size += sizeof(prediction.player_health);
    final_size += sizeof(prediction.ws_position);
    final_size += sizeof(prediction.ws_view_direction);
    final_size += sizeof(prediction.ws_up_vector);
    final_size += sizeof(prediction.ws_velocity);

    final_size += sizeof(prediction.chunk_mod_count);
    for (uint32_t c = 0; c < prediction.chunk_mod_count; ++c) {
        // Number of modified voxels in this chunk
        final_size += sizeof(chunk_modifications_t::modified_voxels_count);
        // Size of the coordinates of this chunk
        final_size += sizeof(chunk_modifications_t::x) * 3;

        // Incorporate the size of the actual voxel values
        uint32_t sizeof_voxel_modification = sizeof(voxel_modification_t::index) + sizeof(voxel_modification_t::initial_value) + sizeof(voxel_modification_t::final_value);
        final_size += prediction.chunk_modifications[c].modified_voxels_count * sizeof_voxel_modification;

        // Incorporate the size of the colors
        final_size += prediction.chunk_modifications[c].modified_voxels_count * sizeof(vkph::voxel_color_t);
    }

    final_size += sizeof(predicted_hit_count);

    uint32_t predicted_hit_size =
        sizeof(vkph::predicted_projectile_hit_t::client_id) +
        sizeof(vkph::predicted_projectile_hit_t::progression) +
        sizeof(vkph::predicted_projectile_hit_t::tick_before) +
        sizeof(vkph::predicted_projectile_hit_t::tick_after);

    final_size += predicted_hit_size * predicted_hit_count;

    return final_size;
}

void packet_client_commands_t::serialise(serialiser_t *serialiser) {
    serialiser->serialise_uint8(flags);
    serialiser->serialise_uint8(command_count);

    for (uint32_t i = 0; i < command_count; ++i) {
        serialiser->serialise_uint16(actions[i].bytes);
        serialiser->serialise_float32(actions[i].dmouse_x);
        serialiser->serialise_float32(actions[i].dmouse_y);
        serialiser->serialise_float32(actions[i].dt);
        serialiser->serialise_float32(actions[i].accumulated_dt);
        serialiser->serialise_uint64(actions[i].tick);
    }

    serialiser->serialise_uint32(prediction.player_flags.u32);
    serialiser->serialise_uint32(prediction.player_health);

    serialiser->serialise_vector3(prediction.ws_position);
    serialiser->serialise_vector3(prediction.ws_view_direction);
    serialiser->serialise_vector3(prediction.ws_up_vector);

    serialiser->serialise_vector3(prediction.ws_velocity);

    serialiser->serialise_uint32(prediction.chunk_mod_count);

    for (uint32_t i = 0; i < prediction.chunk_mod_count; ++i) {
        chunk_modifications_t *c = &prediction.chunk_modifications[i];
        serialise_chunk_modification_meta_info(serialiser, c);
        serialise_chunk_modification_values_with_initial_values(serialiser, c);
        serialise_chunk_modification_colors_from_array(serialiser, c);
    }

    serialiser->serialise_uint32(predicted_hit_count);

    for (uint32_t i = 0; i < predicted_hit_count; ++i) {
        serialiser->serialise_uint16(hits[i].client_id);
        serialiser->serialise_float32(hits[i].progression);
        serialiser->serialise_uint64(hits[i].tick_before);
        serialiser->serialise_uint64(hits[i].tick_after);
    }
}

void packet_client_commands_t::deserialise(serialiser_t *serialiser) {
    flags = serialiser->deserialise_uint8();
    command_count = serialiser->deserialise_uint8();

    actions = LN_MALLOC(vkph::player_action_t, command_count);
    for (uint32_t i = 0; i < command_count; ++i) {
        actions[i].bytes = serialiser->deserialise_uint16();
        actions[i].dmouse_x = serialiser->deserialise_float32();
        actions[i].dmouse_y = serialiser->deserialise_float32();
        actions[i].dt = serialiser->deserialise_float32();
        actions[i].accumulated_dt = serialiser->deserialise_float32();
        actions[i].tick = serialiser->deserialise_uint64();
    }

    prediction.player_flags.u32 = serialiser->deserialise_uint32();
    prediction.player_health = serialiser->deserialise_uint32();

    prediction.ws_position = serialiser->deserialise_vector3();
    prediction.ws_view_direction = serialiser->deserialise_vector3();
    prediction.ws_up_vector = serialiser->deserialise_vector3();

    prediction.ws_velocity = serialiser->deserialise_vector3();

    prediction.chunk_mod_count = serialiser->deserialise_uint32();
    prediction.chunk_modifications = LN_MALLOC(chunk_modifications_t, prediction.chunk_mod_count);

    for (uint32_t i = 0; i < prediction.chunk_mod_count; ++i) {
        chunk_modifications_t *c = &prediction.chunk_modifications[i];
        deserialise_chunk_modification_meta_info(serialiser, c);
        deserialise_chunk_modification_values_with_initial_values(serialiser, c);
        deserialise_chunk_modification_colors_from_array(serialiser, c);
    }

    predicted_hit_count = serialiser->deserialise_uint32();
    hits = LN_MALLOC(vkph::predicted_projectile_hit_t, predicted_hit_count);

    for (uint32_t i = 0; i < predicted_hit_count; ++i) {
        hits[i].client_id = serialiser->deserialise_uint16();
        hits[i].progression = serialiser->deserialise_float32();
        hits[i].tick_before = serialiser->deserialise_uint64();
        hits[i].tick_after = serialiser->deserialise_uint64();
    }
}

uint32_t packet_game_state_snapshot_t::size() {
    uint32_t final_size = 0;
    final_size += sizeof(packet_game_state_snapshot_t::player_data_count);

    uint32_t player_snapshot_size =
        sizeof(vkph::player_snapshot_t::flags) +
        sizeof(vkph::player_snapshot_t::client_id) +
        sizeof(vkph::player_snapshot_t::player_local_flags) +
        sizeof(vkph::player_snapshot_t::player_health) +
        sizeof(vkph::player_snapshot_t::ws_position) +
        sizeof(vkph::player_snapshot_t::ws_view_direction) +
        sizeof(vkph::player_snapshot_t::ws_up_vector) +
        sizeof(vkph::player_snapshot_t::ws_next_random_spawn) +
        sizeof(vkph::player_snapshot_t::ws_velocity) +
        sizeof(vkph::player_snapshot_t::frame_displacement) +
        sizeof(vkph::player_snapshot_t::tick) +
        sizeof(vkph::player_snapshot_t::terraform_tick);

    final_size += player_snapshot_size * player_data_count;

    uint32_t rock_snapshot_size =
        sizeof(vkph::rock_snapshot_t::position) +
        sizeof(vkph::rock_snapshot_t::direction) +
        sizeof(vkph::rock_snapshot_t::up) +
        sizeof(vkph::rock_snapshot_t::client_id);

    final_size += rock_snapshot_size * rock_count;

    return final_size;
}

void packet_game_state_snapshot_t::serialise(serialiser_t *serialiser) {
    serialiser->serialise_uint32(player_data_count);
    for (uint32_t i = 0; i < player_data_count; ++i) {
        serialiser->serialise_uint16(player_snapshots[i].flags);
        serialiser->serialise_uint16(player_snapshots[i].client_id);
        serialiser->serialise_uint32(player_snapshots[i].player_local_flags);
        serialiser->serialise_uint32(player_snapshots[i].player_health);
        serialiser->serialise_vector3(player_snapshots[i].ws_position);
        serialiser->serialise_vector3(player_snapshots[i].ws_view_direction);
        serialiser->serialise_vector3(player_snapshots[i].ws_up_vector);
        serialiser->serialise_vector3(player_snapshots[i].ws_next_random_spawn);
        serialiser->serialise_vector3(player_snapshots[i].ws_velocity);
        serialiser->serialise_float32(player_snapshots[i].frame_displacement);
        serialiser->serialise_uint64(player_snapshots[i].tick);
        serialiser->serialise_uint64(player_snapshots[i].terraform_tick);
    }

    serialiser->serialise_uint32(rock_count);
    for (uint32_t i = 0; i < rock_count; ++i) {
        serialiser->serialise_vector3(rock_snapshots[i].position);
        serialiser->serialise_vector3(rock_snapshots[i].direction);
        serialiser->serialise_vector3(rock_snapshots[i].up);
        serialiser->serialise_uint16(rock_snapshots[i].client_id);
    }
}

void packet_game_state_snapshot_t::deserialise(serialiser_t *serialiser) {
    player_data_count = serialiser->deserialise_uint32();
    player_snapshots = LN_MALLOC(vkph::player_snapshot_t, player_data_count);

    for (uint32_t i = 0; i < player_data_count; ++i) {
        player_snapshots[i].flags = serialiser->deserialise_uint16();
        player_snapshots[i].client_id = serialiser->deserialise_uint16();
        player_snapshots[i].player_local_flags = serialiser->deserialise_uint32();
        player_snapshots[i].player_health = serialiser->deserialise_uint32();
        player_snapshots[i].ws_position = serialiser->deserialise_vector3();
        player_snapshots[i].ws_view_direction = serialiser->deserialise_vector3();
        player_snapshots[i].ws_up_vector = serialiser->deserialise_vector3();
        player_snapshots[i].ws_next_random_spawn = serialiser->deserialise_vector3();
        player_snapshots[i].ws_velocity = serialiser->deserialise_vector3();
        player_snapshots[i].frame_displacement = serialiser->deserialise_float32();
        player_snapshots[i].tick = serialiser->deserialise_uint64();
        player_snapshots[i].terraform_tick = serialiser->deserialise_uint64();
    }

    rock_count = serialiser->deserialise_uint32();
    rock_snapshots = LN_MALLOC(vkph::rock_snapshot_t, rock_count);

    for (uint32_t i = 0; i < rock_count; ++i) {
        rock_snapshots[i].position = serialiser->deserialise_vector3();
        rock_snapshots[i].direction = serialiser->deserialise_vector3();
        rock_snapshots[i].up = serialiser->deserialise_vector3();
        rock_snapshots[i].client_id = serialiser->deserialise_uint16();
    }
}

uint32_t packet_player_team_change_t::size() {
    return sizeof(client_id) + sizeof(color);
}

void packet_player_team_change_t::serialise(serialiser_t *serialiser) {
    serialiser->serialise_uint16(client_id);
    serialiser->serialise_uint16(color);
}
    
void packet_player_team_change_t::deserialise(serialiser_t *serialiser) {
    client_id = serialiser->deserialise_uint16();
    color = serialiser->deserialise_uint16();
}

packet_t get_next_packet_udp(context_t *ctx) {
    packet_t packet = {};

    packet.from = {};
    packet.bytes_received = ctx->main_udp_recv_from(
        ctx->message_buffer, sizeof(char) * net::NET_MAX_MESSAGE_SIZE,
        &packet.from);

    packet.serialiser.data_buffer = (uint8_t *)ctx->message_buffer;
    packet.serialiser.data_buffer_size = packet.bytes_received;

    packet.header.deserialise(&packet.serialiser);

    return packet;
}

packet_t get_next_packet_tcp(socket_t sock, context_t *ctx) {
    packet_t packet = {};

    packet.from = {};
    packet.bytes_received = receive_from_bound_address(
        sock,
        ctx->message_buffer,
        sizeof(char) * net::NET_MAX_MESSAGE_SIZE);

    packet.serialiser.data_buffer = (uint8_t *)ctx->message_buffer;
    packet.serialiser.data_buffer_size = packet.bytes_received;

    packet.header.deserialise(&packet.serialiser);

    return packet;
}

}
