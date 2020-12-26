#include "net.hpp"
#include "allocators.hpp"
#include "game_packet.hpp"

uint32_t packed_packet_header_size() {
    return
        sizeof(packet_header_t::flags) +
        sizeof(packet_header_t::current_tick) +
        sizeof(packet_header_t::current_packet_count) +
        sizeof(packet_header_t::client_id);
}

uint32_t packed_connection_request_size(
    packet_connection_request_t *connection_request) {
    return sizeof(uint8_t) * (uint32_t)strlen(connection_request->name);
}

void serialise_packet_header(
    packet_header_t *header,
    serialiser_t *serialiser) {
    serialiser->serialise_uint32(header->flags.bytes);
    serialiser->serialise_uint64(header->current_tick);
    serialiser->serialise_uint64(header->current_packet_count);
    serialiser->serialise_uint16(header->client_id);
}

void deserialise_packet_header(
    packet_header_t *header,
    serialiser_t *serialiser) {
    header->flags.bytes = serialiser->deserialise_uint32();
    header->current_tick = serialiser->deserialise_uint64();
    header->current_packet_count = serialiser->deserialise_uint64();
    header->client_id = serialiser->deserialise_uint16();
}

void serialise_connection_request(
    packet_connection_request_t *packet,
    serialiser_t *serialiser) {
    serialiser->serialise_string(packet->name);
}

void deserialise_connection_request(
    packet_connection_request_t *request,
    serialiser_t *serialiser) {
    request->name = serialiser->deserialise_string();
}

uint32_t packed_connection_handshake_size(
    packet_connection_handshake_t *game_state) {
    uint32_t final_size = 0;
    final_size += sizeof(game_state->player_count);
    final_size += game_state->player_count * sizeof(full_player_info_t);

    // Other information...
    return final_size;
}

void serialise_connection_handshake(
    packet_connection_handshake_t *full_game_state,
    serialiser_t *serialiser) {
    serialiser->serialise_uint32(full_game_state->loaded_chunk_count);
    serialiser->serialise_uint32(full_game_state->player_count);
    for (uint32_t i = 0; i < full_game_state->player_count; ++i) {
        serialiser->serialise_string(full_game_state->player_infos[i].name);
        serialiser->serialise_uint16(full_game_state->player_infos[i].client_id);
        serialiser->serialise_vector3(full_game_state->player_infos[i].ws_position);
        serialiser->serialise_vector3(full_game_state->player_infos[i].ws_view_direction);
        serialiser->serialise_vector3(full_game_state->player_infos[i].ws_up_vector);
        serialiser->serialise_vector3(full_game_state->player_infos[i].ws_next_random_position);
        serialiser->serialise_float32(full_game_state->player_infos[i].default_speed);
        serialiser->serialise_uint32(full_game_state->player_infos[i].flags.u32);
    }

    serialiser->serialise_uint32(full_game_state->team_count);
    for (uint32_t i = 0; i < full_game_state->team_count; ++i) {
        serialiser->serialise_uint32(full_game_state->team_infos[i].color);
        serialiser->serialise_uint32(full_game_state->team_infos[i].player_count);
        serialiser->serialise_uint32(full_game_state->team_infos[i].max_players);
    }
}

void deserialise_connection_handshake(
    packet_connection_handshake_t *full_game_state,
    serialiser_t *serialiser) {
    full_game_state->loaded_chunk_count = serialiser->deserialise_uint32();
    full_game_state->player_count = serialiser->deserialise_uint32();
    full_game_state->player_infos = LN_MALLOC(full_player_info_t, full_game_state->player_count);

    for (uint32_t i = 0; i < full_game_state->player_count; ++i) {
        full_game_state->player_infos[i].name = serialiser->deserialise_string();
        full_game_state->player_infos[i].client_id = serialiser->deserialise_uint16();
        full_game_state->player_infos[i].ws_position = serialiser->deserialise_vector3();
        full_game_state->player_infos[i].ws_view_direction = serialiser->deserialise_vector3();
        full_game_state->player_infos[i].ws_up_vector = serialiser->deserialise_vector3();
        full_game_state->player_infos[i].ws_next_random_position = serialiser->deserialise_vector3();
        full_game_state->player_infos[i].default_speed = serialiser->deserialise_float32();
        full_game_state->player_infos[i].flags.u32 = serialiser->deserialise_uint32();
    }

    full_game_state->team_count = serialiser->deserialise_uint32();
    full_game_state->team_infos = LN_MALLOC(vkph::team_info_t, full_game_state->team_count);
    for (uint32_t i = 0; i < full_game_state->team_count; ++i) {
        full_game_state->team_infos[i].color = (vkph::team_color_t)serialiser->deserialise_uint32();
        full_game_state->team_infos[i].player_count = serialiser->deserialise_uint32();
        full_game_state->team_infos[i].max_players = serialiser->deserialise_uint32();
    }
}

uint32_t packed_player_joined_size(
    packet_player_joined_t *packet) {
    uint32_t total_size = 0;
    total_size += (uint32_t)strlen(packet->player_info.name);
    total_size += sizeof(full_player_info_t::client_id);
    total_size += sizeof(full_player_info_t::ws_position);
    total_size += sizeof(full_player_info_t::ws_view_direction);
    total_size += sizeof(full_player_info_t::ws_up_vector);
    total_size += sizeof(full_player_info_t::default_speed);
    total_size += sizeof(full_player_info_t::flags);

    return total_size;
}

void serialise_player_joined(
    packet_player_joined_t *packet,
    serialiser_t *serialiser) {
    serialiser->serialise_string(packet->player_info.name);
    serialiser->serialise_uint16(packet->player_info.client_id);
    serialiser->serialise_vector3(packet->player_info.ws_position);
    serialiser->serialise_vector3(packet->player_info.ws_view_direction);
    serialiser->serialise_vector3(packet->player_info.ws_up_vector);
    serialiser->serialise_float32(packet->player_info.default_speed);
    serialiser->serialise_uint8(packet->player_info.flags.u32);
}

void deserialise_player_joined(
    packet_player_joined_t *packet,
    serialiser_t *serialiser) {
    packet->player_info.name = serialiser->deserialise_string();
    packet->player_info.client_id = serialiser->deserialise_uint16();
    packet->player_info.ws_position = serialiser->deserialise_vector3();
    packet->player_info.ws_view_direction = serialiser->deserialise_vector3();
    packet->player_info.ws_up_vector = serialiser->deserialise_vector3();
    packet->player_info.default_speed = serialiser->deserialise_float32();
    packet->player_info.flags.u32 = serialiser->deserialise_uint32();
}

static void s_serialise_chunk_modification_meta_info(
    serialiser_t *serialiser,
    chunk_modifications_t *c) {
    serialiser->serialise_int16(c->x);
    serialiser->serialise_int16(c->y);
    serialiser->serialise_int16(c->z);
    serialiser->serialise_uint32(c->modified_voxels_count);
}

static void s_serialise_chunk_modification_values_without_colors(
    serialiser_t *serialiser,
    chunk_modifications_t *c) {
    for (uint32_t v = 0; v < c->modified_voxels_count; ++v) {
        voxel_modification_t *v_ptr =  &c->modifications[v];
        serialiser->serialise_uint16(v_ptr->index);
        serialiser->serialise_uint8(v_ptr->final_value);
    }
}

static void s_serialise_chunk_modification_values_with_colors(
    serialiser_t *serialiser,
    chunk_modifications_t *c) {
    for (uint32_t v = 0; v < c->modified_voxels_count; ++v) {
        voxel_modification_t *v_ptr =  &c->modifications[v];
        serialiser->serialise_uint16(v_ptr->index);
        serialiser->serialise_uint8(v_ptr->color);
        serialiser->serialise_uint8(v_ptr->final_value);
    }
}

static void s_serialise_chunk_modification_values_with_initial_values(
    serialiser_t *serialiser,
    chunk_modifications_t *c) {
    for (uint32_t v = 0; v < c->modified_voxels_count; ++v) {
        voxel_modification_t *v_ptr =  &c->modifications[v];
        serialiser->serialise_uint16(v_ptr->index);
        serialiser->serialise_uint8(v_ptr->initial_value);
        serialiser->serialise_uint8(v_ptr->final_value);
    }
}

static void s_serialise_chunk_modification_colors_from_array(
    serialiser_t *serialiser,
    chunk_modifications_t *c) {
    for (uint32_t v = 0; v < c->modified_voxels_count; ++v) {
        serialiser->serialise_uint8(c->colors[v]);
    }
}

void serialise_chunk_modifications(
    chunk_modifications_t *modifications,
    uint32_t modification_count,
    serialiser_t *serialiser,
    color_serialisation_type_t cst) {
    serialiser->serialise_uint32(modification_count);
    
    // Yes I know this is stupid because color is a bool
    if (cst == CST_SERIALISE_SEPARATE_COLOR) {
        for (uint32_t i = 0; i < modification_count; ++i) {
            chunk_modifications_t *c = &modifications[i];
            s_serialise_chunk_modification_meta_info(serialiser, c);
            s_serialise_chunk_modification_values_without_colors(serialiser, c);
            s_serialise_chunk_modification_colors_from_array(serialiser, c);
        }
    }
    else {
        for (uint32_t i = 0; i < modification_count; ++i) {
            chunk_modifications_t *c = &modifications[i];
            s_serialise_chunk_modification_meta_info(serialiser, c);
            s_serialise_chunk_modification_values_with_colors(serialiser, c);
        }
    }
}

static void s_deserialise_chunk_modification_meta_info(
    serialiser_t *serialiser,
    chunk_modifications_t *c) {
    c->x = serialiser->deserialise_int16();
    c->y = serialiser->deserialise_int16();
    c->z = serialiser->deserialise_int16();
    c->modified_voxels_count = serialiser->deserialise_uint32();
}

static void s_deserialise_chunk_modification_values_without_colors(
    serialiser_t *serialiser,
    chunk_modifications_t *c) {
    for (uint32_t v = 0; v < c->modified_voxels_count; ++v) {
        voxel_modification_t *v_ptr =  &c->modifications[v];
        v_ptr->index = serialiser->deserialise_uint16();
        v_ptr->final_value = serialiser->deserialise_uint8();
    }
}

static void s_deserialise_chunk_modification_values_with_colors(
    serialiser_t *serialiser,
    chunk_modifications_t *c) {
    for (uint32_t v = 0; v < c->modified_voxels_count; ++v) {
        voxel_modification_t *v_ptr =  &c->modifications[v];
        v_ptr->index = serialiser->deserialise_uint16();
        v_ptr->color = serialiser->deserialise_uint8();
        v_ptr->final_value = serialiser->deserialise_uint8();
    }
}

static void s_deserialise_chunk_modification_values_with_initial_values(
    serialiser_t *serialiser,
    chunk_modifications_t *c) {
    for (uint32_t v = 0; v < c->modified_voxels_count; ++v) {
        voxel_modification_t *v_ptr =  &c->modifications[v];
        v_ptr->index = serialiser->deserialise_uint16();
        v_ptr->initial_value = serialiser->deserialise_uint8();
        v_ptr->final_value = serialiser->deserialise_uint8();
    }
}

static void s_deserialise_chunk_modification_colors_from_array(
    serialiser_t *serialiser,
    chunk_modifications_t *c) {
    for (uint32_t v = 0; v < c->modified_voxels_count; ++v) {
        c->colors[v] = serialiser->deserialise_uint8();
    }
}

chunk_modifications_t *deserialise_chunk_modifications(
    uint32_t *modification_count,
    serialiser_t *serialiser,
    color_serialisation_type_t color) {
    *modification_count = serialiser->deserialise_uint32();
    chunk_modifications_t *chunk_modifications = LN_MALLOC(chunk_modifications_t, *modification_count);

    if (color == CST_SERIALISE_SEPARATE_COLOR) {
        for (uint32_t i = 0; i < *modification_count; ++i) {
            chunk_modifications_t *c = &chunk_modifications[i];
            s_deserialise_chunk_modification_meta_info(serialiser, c);
            s_deserialise_chunk_modification_values_without_colors(serialiser, c);
            s_deserialise_chunk_modification_colors_from_array(serialiser, c);
        }
    }
    else {
        for (uint32_t i = 0; i < *modification_count; ++i) {
            chunk_modifications_t *c = &chunk_modifications[i];
            s_deserialise_chunk_modification_meta_info(serialiser, c);
            s_deserialise_chunk_modification_values_with_colors(serialiser, c);
        }
    }

    return chunk_modifications;
}

uint32_t packed_player_commands_size(
    packet_client_commands_t *commands) {
    uint32_t final_size = 0;
    final_size += sizeof(packet_client_commands_t::command_count);

    uint32_t command_size =
        sizeof(vkph::player_action_t::bytes) +
        sizeof(vkph::player_action_t::dmouse_x) +
        sizeof(vkph::player_action_t::dmouse_y) +
        sizeof(vkph::player_action_t::dt) +
        sizeof(vkph::player_action_t::accumulated_dt);

    final_size += command_size * commands->command_count;

    final_size += sizeof(packet_client_commands_t::player_flags);
    final_size += sizeof(packet_client_commands_t::predicted_health);
    final_size += sizeof(packet_client_commands_t::ws_final_position);
    final_size += sizeof(packet_client_commands_t::ws_final_view_direction);
    final_size += sizeof(packet_client_commands_t::ws_final_up_vector);
    final_size += sizeof(packet_client_commands_t::ws_final_velocity);

    final_size += sizeof(packet_client_commands_t::modified_chunk_count);
    for (uint32_t c = 0; c < commands->modified_chunk_count; ++c) {
        // Number of modified voxels in this chunk
        final_size += sizeof(chunk_modifications_t::modified_voxels_count);
        // Size of the coordinates of this chunk
        final_size += + sizeof(chunk_modifications_t::x) * 3;

        // Incorporate the size of the actual voxel values
        uint32_t sizeof_voxel_modification = sizeof(voxel_modification_t::index) + sizeof(voxel_modification_t::initial_value) + sizeof(voxel_modification_t::final_value);
        final_size += commands->chunk_modifications[c].modified_voxels_count * sizeof_voxel_modification;

        // Incorporate the size of the colors
        final_size += commands->chunk_modifications[c].modified_voxels_count * sizeof(vkph::voxel_color_t);
    }

    final_size += sizeof(packet_client_commands_t::predicted_hit_count);

    uint32_t predicted_hit_size =
        sizeof(vkph::predicted_projectile_hit_t::client_id) +
        sizeof(vkph::predicted_projectile_hit_t::progression) +
        sizeof(vkph::predicted_projectile_hit_t::tick_before) +
        sizeof(vkph::predicted_projectile_hit_t::tick_after);

    final_size += predicted_hit_size * commands->predicted_hit_count;

    return final_size;
}

void serialise_player_commands(
    packet_client_commands_t *packet,
    serialiser_t *serialiser) {
    serialiser->serialise_uint8(packet->flags);
    serialiser->serialise_uint8(packet->command_count);

    for (uint32_t i = 0; i < packet->command_count; ++i) {
        serialiser->serialise_uint16(packet->actions[i].bytes);
        serialiser->serialise_float32(packet->actions[i].dmouse_x);
        serialiser->serialise_float32(packet->actions[i].dmouse_y);
        serialiser->serialise_float32(packet->actions[i].dt);
        serialiser->serialise_float32(packet->actions[i].accumulated_dt);
        serialiser->serialise_uint64(packet->actions[i].tick);
    }

    serialiser->serialise_uint32(packet->player_flags);
    serialiser->serialise_uint32(packet->predicted_health);

    serialiser->serialise_vector3(packet->ws_final_position);
    serialiser->serialise_vector3(packet->ws_final_view_direction);
    serialiser->serialise_vector3(packet->ws_final_up_vector);

    serialiser->serialise_vector3(packet->ws_final_velocity);

    serialiser->serialise_uint32(packet->modified_chunk_count);

    for (uint32_t i = 0; i < packet->modified_chunk_count; ++i) {
        chunk_modifications_t *c = &packet->chunk_modifications[i];
        s_serialise_chunk_modification_meta_info(serialiser, c);
        s_serialise_chunk_modification_values_with_initial_values(serialiser, c);
        s_serialise_chunk_modification_colors_from_array(serialiser, c);
    }

    serialiser->serialise_uint32(packet->predicted_hit_count);

    for (uint32_t i = 0; i < packet->predicted_hit_count; ++i) {
        serialiser->serialise_uint16(packet->hits[i].client_id);
        serialiser->serialise_float32(packet->hits[i].progression);
        serialiser->serialise_uint64(packet->hits[i].tick_before);
        serialiser->serialise_uint64(packet->hits[i].tick_after);
    }
}

void deserialise_player_commands(
    packet_client_commands_t *packet,
    serialiser_t *serialiser) {
    packet->flags = serialiser->deserialise_uint8();
    packet->command_count = serialiser->deserialise_uint8();

    packet->actions = LN_MALLOC(vkph::player_action_t, packet->command_count);
    for (uint32_t i = 0; i < packet->command_count; ++i) {
        packet->actions[i].bytes = serialiser->deserialise_uint16();
        packet->actions[i].dmouse_x = serialiser->deserialise_float32();
        packet->actions[i].dmouse_y = serialiser->deserialise_float32();
        packet->actions[i].dt = serialiser->deserialise_float32();
        packet->actions[i].accumulated_dt = serialiser->deserialise_float32();
        packet->actions[i].tick = serialiser->deserialise_uint64();
    }

    packet->player_flags = serialiser->deserialise_uint32();
    packet->predicted_health = serialiser->deserialise_uint32();

    packet->ws_final_position = serialiser->deserialise_vector3();
    packet->ws_final_view_direction = serialiser->deserialise_vector3();
    packet->ws_final_up_vector = serialiser->deserialise_vector3();

    packet->ws_final_velocity = serialiser->deserialise_vector3();

    packet->modified_chunk_count = serialiser->deserialise_uint32();
    packet->chunk_modifications = LN_MALLOC(chunk_modifications_t, packet->modified_chunk_count);

    for (uint32_t i = 0; i < packet->modified_chunk_count; ++i) {
        chunk_modifications_t *c = &packet->chunk_modifications[i];
        s_deserialise_chunk_modification_meta_info(serialiser, c);
        s_deserialise_chunk_modification_values_with_initial_values(serialiser, c);
        s_deserialise_chunk_modification_colors_from_array(serialiser, c);
    }

    packet->predicted_hit_count = serialiser->deserialise_uint32();
    packet->hits = LN_MALLOC(vkph::predicted_projectile_hit_t, packet->predicted_hit_count);

    for (uint32_t i = 0; i < packet->predicted_hit_count; ++i) {
        packet->hits[i].client_id = serialiser->deserialise_uint16();
        packet->hits[i].progression = serialiser->deserialise_float32();
        packet->hits[i].tick_before = serialiser->deserialise_uint64();
        packet->hits[i].tick_after = serialiser->deserialise_uint64();
    }
}

uint32_t packed_game_state_snapshot_size(
    packet_game_state_snapshot_t *packet) {
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

    final_size += player_snapshot_size * packet->player_data_count;

    uint32_t rock_snapshot_size =
        sizeof(vkph::rock_snapshot_t::position) +
        sizeof(vkph::rock_snapshot_t::direction) +
        sizeof(vkph::rock_snapshot_t::up) +
        sizeof(vkph::rock_snapshot_t::client_id);

    final_size += rock_snapshot_size * packet->rock_count;

    return final_size;
}

void serialise_game_state_snapshot(
    packet_game_state_snapshot_t *packet,
    serialiser_t *serialiser) {
    serialiser->serialise_uint32(packet->player_data_count);
    for (uint32_t i = 0; i < packet->player_data_count; ++i) {
        serialiser->serialise_uint16(packet->player_snapshots[i].flags);
        serialiser->serialise_uint16(packet->player_snapshots[i].client_id);
        serialiser->serialise_uint32(packet->player_snapshots[i].player_local_flags);
        serialiser->serialise_uint32(packet->player_snapshots[i].player_health);
        serialiser->serialise_vector3(packet->player_snapshots[i].ws_position);
        serialiser->serialise_vector3(packet->player_snapshots[i].ws_view_direction);
        serialiser->serialise_vector3(packet->player_snapshots[i].ws_up_vector);
        serialiser->serialise_vector3(packet->player_snapshots[i].ws_next_random_spawn);
        serialiser->serialise_vector3(packet->player_snapshots[i].ws_velocity);
        serialiser->serialise_float32(packet->player_snapshots[i].frame_displacement);
        serialiser->serialise_uint64(packet->player_snapshots[i].tick);
        serialiser->serialise_uint64(packet->player_snapshots[i].terraform_tick);
    }

    serialiser->serialise_uint32(packet->rock_count);
    for (uint32_t i = 0; i < packet->rock_count; ++i) {
        serialiser->serialise_vector3(packet->rock_snapshots[i].position);
        serialiser->serialise_vector3(packet->rock_snapshots[i].direction);
        serialiser->serialise_vector3(packet->rock_snapshots[i].up);
        serialiser->serialise_uint16(packet->rock_snapshots[i].client_id);
    }
}

void deserialise_game_state_snapshot(
    packet_game_state_snapshot_t *packet,
    serialiser_t *serialiser) {
    packet->player_data_count = serialiser->deserialise_uint32();
    packet->player_snapshots = LN_MALLOC(vkph::player_snapshot_t, packet->player_data_count);

    for (uint32_t i = 0; i < packet->player_data_count; ++i) {
        packet->player_snapshots[i].flags = serialiser->deserialise_uint16();
        packet->player_snapshots[i].client_id = serialiser->deserialise_uint16();
        packet->player_snapshots[i].player_local_flags = serialiser->deserialise_uint32();
        packet->player_snapshots[i].player_health = serialiser->deserialise_uint32();
        packet->player_snapshots[i].ws_position = serialiser->deserialise_vector3();
        packet->player_snapshots[i].ws_view_direction = serialiser->deserialise_vector3();
        packet->player_snapshots[i].ws_up_vector = serialiser->deserialise_vector3();
        packet->player_snapshots[i].ws_next_random_spawn = serialiser->deserialise_vector3();
        packet->player_snapshots[i].ws_velocity = serialiser->deserialise_vector3();
        packet->player_snapshots[i].frame_displacement = serialiser->deserialise_float32();
        packet->player_snapshots[i].tick = serialiser->deserialise_uint64();
        packet->player_snapshots[i].terraform_tick = serialiser->deserialise_uint64();
    }

    packet->rock_count = serialiser->deserialise_uint32();
    packet->rock_snapshots = LN_MALLOC(vkph::rock_snapshot_t, packet->rock_count);

    for (uint32_t i = 0; i < packet->rock_count; ++i) {
        packet->rock_snapshots[i].position = serialiser->deserialise_vector3();
        packet->rock_snapshots[i].direction = serialiser->deserialise_vector3();
        packet->rock_snapshots[i].up = serialiser->deserialise_vector3();
        packet->rock_snapshots[i].client_id = serialiser->deserialise_uint16();
    }
}

uint32_t packed_chunk_voxels_size(
    packet_chunk_voxels_t *packet) {
    uint32_t final_size = 0;
    final_size += sizeof(packet_chunk_voxels_t::chunk_in_packet_count);

    using vkph::CHUNK_EDGE_LENGTH;

    uint32_t voxel_chunk_values_size =
        3 * sizeof(int16_t) + CHUNK_EDGE_LENGTH * CHUNK_EDGE_LENGTH * CHUNK_EDGE_LENGTH * sizeof(vkph::voxel_t);

    final_size += voxel_chunk_values_size * packet->chunk_in_packet_count;

    return final_size;
}

void serialise_packet_chunk_voxels(
    packet_chunk_voxels_t *packet,
    serialiser_t *serialiser) {
    serialiser->serialise_uint32(packet->chunk_in_packet_count);

    using vkph::CHUNK_EDGE_LENGTH;

    for (uint32_t i = 0; i < packet->chunk_in_packet_count; ++i) {
        serialiser->serialise_int16(packet->values[i].x);
        serialiser->serialise_int16(packet->values[i].y);
        serialiser->serialise_int16(packet->values[i].z);
        // TODO: In future, optimise this, use the fact that the maximum value for a voxel is 254.
        // Make 255 a marker for: no more values that are not 0 or something
        serialiser->serialise_bytes((uint8_t *)packet->values[i].voxel_values, CHUNK_EDGE_LENGTH * CHUNK_EDGE_LENGTH * CHUNK_EDGE_LENGTH * sizeof(vkph::voxel_t));
    }
}

void deserialise_packet_chunk_voxels(
    packet_chunk_voxels_t *packet,
    serialiser_t *serialiser) {
    packet->chunk_in_packet_count = serialiser->deserialise_uint32();

    packet->values = LN_MALLOC(voxel_chunk_values_t, packet->chunk_in_packet_count);

    using vkph::CHUNK_EDGE_LENGTH;

    for (uint32_t i = 0; i < packet->chunk_in_packet_count; ++i) {
        packet->values[i].x = serialiser->deserialise_int16();
        packet->values[i].y = serialiser->deserialise_int16();
        packet->values[i].z = serialiser->deserialise_int16();

        packet->values[i].voxel_values = (vkph::voxel_t *)serialiser->deserialise_bytes(NULL, CHUNK_EDGE_LENGTH * CHUNK_EDGE_LENGTH * CHUNK_EDGE_LENGTH * sizeof(vkph::voxel_t));
    }
}

uint32_t packed_player_team_change_size() {
    return sizeof(packet_player_team_change_t::client_id) + sizeof(packet_player_team_change_t::color);
}

void serialise_packet_player_team_change(packet_player_team_change_t *packet, serialiser_t *serialiser) {
    serialiser->serialise_uint16(packet->client_id);
    serialiser->serialise_uint16(packet->color);
}
    
void deserialise_packet_player_team_change(packet_player_team_change_t *packet, serialiser_t *serialiser) {
    packet->client_id = serialiser->deserialise_uint16();
    packet->color = serialiser->deserialise_uint16();
}
