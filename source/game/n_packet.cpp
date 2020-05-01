#include "world.hpp"
#include "n_internal.hpp"
#include <common/allocators.hpp>

uint32_t n_packed_packet_header_size() {
    return
        sizeof(packet_header_t::flags) +
        sizeof(packet_header_t::current_tick) +
        sizeof(packet_header_t::current_packet_count) +
        sizeof(packet_header_t::client_id);
}

uint32_t n_packed_connection_request_size(
    packet_connection_request_t *connection_request) {
    return sizeof(uint8_t) * (uint32_t)strlen(connection_request->name);
}

void n_serialise_packet_header(
    packet_header_t *header,
    serialiser_t *serialiser) {
    serialiser->serialise_uint32(header->flags.bytes);
    serialiser->serialise_uint64(header->current_tick);
    serialiser->serialise_uint64(header->current_packet_count);
    serialiser->serialise_uint16(header->client_id);
}

void n_deserialise_packet_header(
    packet_header_t *header,
    serialiser_t *serialiser) {
    header->flags.bytes = serialiser->deserialise_uint32();
    header->current_tick = serialiser->deserialise_uint64();
    header->current_packet_count = serialiser->deserialise_uint64();
    header->client_id = serialiser->deserialise_uint16();
}

void n_serialise_connection_request(
    packet_connection_request_t *packet,
    serialiser_t *serialiser) {
    serialiser->serialise_string(packet->name);
}

void n_deserialise_connection_request(
    packet_connection_request_t *request,
    serialiser_t *serialiser) {
    request->name = serialiser->deserialise_string();
}

uint32_t n_packed_connection_handshake_size(
    packet_connection_handshake_t *game_state) {
    uint32_t final_size = 0;
    final_size += sizeof(game_state->player_count);
    final_size += game_state->player_count * sizeof(full_player_info_t);

    // Other information...
    return final_size;
}

void n_serialise_connection_handshake(
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
        serialiser->serialise_float32(full_game_state->player_infos[i].default_speed);
        serialiser->serialise_uint32(full_game_state->player_infos[i].flags);
    }
}

void n_deserialise_connection_handshake(
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
        full_game_state->player_infos[i].default_speed = serialiser->deserialise_float32();
        full_game_state->player_infos[i].flags = serialiser->deserialise_uint32();
    }
}

uint32_t n_packed_player_joined_size(
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

void n_serialise_player_joined(
    packet_player_joined_t *packet,
    serialiser_t *serialiser) {
    serialiser->serialise_string(packet->player_info.name);
    serialiser->serialise_uint16(packet->player_info.client_id);
    serialiser->serialise_vector3(packet->player_info.ws_position);
    serialiser->serialise_vector3(packet->player_info.ws_view_direction);
    serialiser->serialise_vector3(packet->player_info.ws_up_vector);
    serialiser->serialise_float32(packet->player_info.default_speed);
    serialiser->serialise_uint8(packet->player_info.flags);
}

void n_deserialise_player_joined(
    packet_player_joined_t *packet,
    serialiser_t *serialiser) {
    packet->player_info.name = serialiser->deserialise_string();
    packet->player_info.client_id = serialiser->deserialise_uint16();
    packet->player_info.ws_position = serialiser->deserialise_vector3();
    packet->player_info.ws_view_direction = serialiser->deserialise_vector3();
    packet->player_info.ws_up_vector = serialiser->deserialise_vector3();
    packet->player_info.default_speed = serialiser->deserialise_float32();
    packet->player_info.flags = serialiser->deserialise_uint32();
}

uint32_t n_packed_player_commands_size(
    packet_player_commands_t *commands) {
    uint32_t final_size = 0;
    final_size += sizeof(packet_player_commands_t::command_count);

    uint32_t command_size =
        sizeof(player_actions_t::bytes) +
        sizeof(player_actions_t::dmouse_x) +
        sizeof(player_actions_t::dmouse_y) +
        sizeof(player_actions_t::dt) +
        sizeof(player_actions_t::accumulated_dt);

    final_size += command_size * commands->command_count;

    final_size += sizeof(packet_player_commands_t::modified_chunk_count);
    for (uint32_t c = 0; c < commands->modified_chunk_count; ++c) {
        final_size += sizeof(chunk_modifications_t::modified_voxels_count) + sizeof(chunk_modifications_t::x) * 3;

        final_size += commands->chunk_modifications[c].modified_voxels_count * (sizeof(voxel_modification_t::index) + sizeof(voxel_modification_t::final_value));
    }

    return final_size;
}

void n_serialise_player_commands(
    packet_player_commands_t *packet,
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

    serialiser->serialise_vector3(packet->ws_final_position);
    serialiser->serialise_vector3(packet->ws_final_view_direction);
    serialiser->serialise_vector3(packet->ws_final_up_vector);

    serialiser->serialise_uint32(packet->modified_chunk_count);

    for (uint32_t i = 0; i < packet->modified_chunk_count; ++i) {
        chunk_modifications_t *c = &packet->chunk_modifications[i];
        serialiser->serialise_int16(c->x);
        serialiser->serialise_int16(c->y);
        serialiser->serialise_int16(c->z);
        serialiser->serialise_uint32(c->modified_voxels_count);

        for (uint32_t v = 0; v < c->modified_voxels_count; ++v) {
            voxel_modification_t *v_ptr =  &c->modifications[v];
            serialiser->serialise_uint16(v_ptr->index);
            serialiser->serialise_uint8(v_ptr->initial_value);
            serialiser->serialise_uint8(v_ptr->final_value);
        }
    }
}

void n_deserialise_player_commands(
    packet_player_commands_t *packet,
    serialiser_t *serialiser) {
    packet->flags = serialiser->deserialise_uint8();
    packet->command_count = serialiser->deserialise_uint8();

    packet->actions = LN_MALLOC(player_actions_t, packet->command_count);
    for (uint32_t i = 0; i < packet->command_count; ++i) {
        packet->actions[i].bytes = serialiser->deserialise_uint16();
        packet->actions[i].dmouse_x = serialiser->deserialise_float32();
        packet->actions[i].dmouse_y = serialiser->deserialise_float32();
        packet->actions[i].dt = serialiser->deserialise_float32();
        packet->actions[i].accumulated_dt = serialiser->deserialise_float32();
        packet->actions[i].tick = serialiser->deserialise_uint64();
    }

    packet->ws_final_position = serialiser->deserialise_vector3();
    packet->ws_final_view_direction = serialiser->deserialise_vector3();
    packet->ws_final_up_vector = serialiser->deserialise_vector3();

    packet->modified_chunk_count = serialiser->deserialise_uint32();
    packet->chunk_modifications = LN_MALLOC(chunk_modifications_t, packet->modified_chunk_count);

    for (uint32_t i = 0; i < packet->modified_chunk_count; ++i) {
        chunk_modifications_t *c = &packet->chunk_modifications[i];
        c->x = serialiser->deserialise_int16();
        c->y = serialiser->deserialise_int16();
        c->z = serialiser->deserialise_int16();
        c->modified_voxels_count = serialiser->deserialise_uint32();

        for (uint32_t v = 0; v < c->modified_voxels_count; ++v) {
            voxel_modification_t *v_ptr =  &c->modifications[v];
            v_ptr->index = serialiser->deserialise_uint16();
            v_ptr->initial_value = serialiser->deserialise_uint8();
            v_ptr->final_value = serialiser->deserialise_uint8();
        }
    }
}

uint32_t n_packed_game_state_snapshot_size(
    packet_game_state_snapshot_t *packet) {
    uint32_t final_size = 0;
    final_size += sizeof(packet_game_state_snapshot_t::player_data_count);

    uint32_t player_snapshot_size =
        sizeof(player_snapshot_t::flags) +
        sizeof(player_snapshot_t::client_id) +
        sizeof(player_snapshot_t::ws_position) +
        sizeof(player_snapshot_t::ws_view_direction) +
        sizeof(player_snapshot_t::ws_up_vector) +
        sizeof(player_snapshot_t::tick) +
        sizeof(player_snapshot_t::terraform_tick);

    final_size += player_snapshot_size * packet->player_data_count;

    return final_size;
}

void n_serialise_game_state_snapshot(
    packet_game_state_snapshot_t *packet,
    serialiser_t *serialiser) {
    serialiser->serialise_uint32(packet->player_data_count);
    for (uint32_t i = 0; i < packet->player_data_count; ++i) {
        serialiser->serialise_uint8(packet->player_snapshots[i].flags);
        serialiser->serialise_uint16(packet->player_snapshots[i].client_id);
        serialiser->serialise_vector3(packet->player_snapshots[i].ws_position);
        serialiser->serialise_vector3(packet->player_snapshots[i].ws_view_direction);
        serialiser->serialise_vector3(packet->player_snapshots[i].ws_up_vector);
        serialiser->serialise_uint64(packet->player_snapshots[i].tick);
        serialiser->serialise_uint64(packet->player_snapshots[i].terraform_tick);
    }
}

void n_deserialise_game_state_snapshot(
    packet_game_state_snapshot_t *packet,
    serialiser_t *serialiser) {
    packet->player_data_count = serialiser->deserialise_uint32();
    packet->player_snapshots = LN_MALLOC(player_snapshot_t, packet->player_data_count);

    for (uint32_t i = 0; i < packet->player_data_count; ++i) {
        packet->player_snapshots[i].flags = serialiser->deserialise_uint8();
        packet->player_snapshots[i].client_id = serialiser->deserialise_uint16();
        packet->player_snapshots[i].ws_position = serialiser->deserialise_vector3();
        packet->player_snapshots[i].ws_view_direction = serialiser->deserialise_vector3();
        packet->player_snapshots[i].ws_up_vector = serialiser->deserialise_vector3();
        packet->player_snapshots[i].tick = serialiser->deserialise_uint64();
        packet->player_snapshots[i].terraform_tick = serialiser->deserialise_uint64();
    }
}

void n_serialise_chunk_modifications(
    chunk_modifications_t *modifications,
    uint32_t modification_count,
    serialiser_t *serialiser) {
    serialiser->serialise_uint32(modification_count);
    
    for (uint32_t i = 0; i < modification_count; ++i) {
        chunk_modifications_t *c = &modifications[i];
        serialiser->serialise_int16(c->x);
        serialiser->serialise_int16(c->y);
        serialiser->serialise_int16(c->z);
        serialiser->serialise_uint32(c->modified_voxels_count);

        for (uint32_t v = 0; v < c->modified_voxels_count; ++v) {
            voxel_modification_t *v_ptr =  &c->modifications[v];
            serialiser->serialise_uint16(v_ptr->index);
            serialiser->serialise_uint8(v_ptr->final_value);
        }
    }
}

chunk_modifications_t *n_deserialise_chunk_modifications(
    uint32_t *modification_count,
    serialiser_t *serialiser) {
    *modification_count = serialiser->deserialise_uint32();
    chunk_modifications_t *chunk_modifications = LN_MALLOC(chunk_modifications_t, *modification_count);

    for (uint32_t i = 0; i < *modification_count; ++i) {
        chunk_modifications_t *c = &chunk_modifications[i];
        c->x = serialiser->deserialise_int16();
        c->y = serialiser->deserialise_int16();
        c->z = serialiser->deserialise_int16();
        c->modified_voxels_count = serialiser->deserialise_uint32();

        for (uint32_t v = 0; v < c->modified_voxels_count; ++v) {
            voxel_modification_t *v_ptr =  &c->modifications[v];
            v_ptr->index = serialiser->deserialise_uint16();
            v_ptr->final_value = serialiser->deserialise_uint8();
        }
    }

    return chunk_modifications;
}

uint32_t n_packed_chunk_voxels_size(
    packet_chunk_voxels_t *packet) {
    uint32_t final_size = 0;
    final_size += sizeof(packet_chunk_voxels_t::chunk_in_packet_count);

    uint32_t voxel_chunk_values_size = 3 * sizeof(int16_t) + CHUNK_EDGE_LENGTH * CHUNK_EDGE_LENGTH * CHUNK_EDGE_LENGTH * sizeof(uint8_t);

    final_size += voxel_chunk_values_size * packet->chunk_in_packet_count;

    return final_size;
}

void n_serialise_packet_chunk_voxels(
    packet_chunk_voxels_t *packet,
    serialiser_t *serialiser) {
    serialiser->serialise_uint32(packet->chunk_in_packet_count);

    for (uint32_t i = 0; i < packet->chunk_in_packet_count; ++i) {
        serialiser->serialise_int16(packet->values[i].x);
        serialiser->serialise_int16(packet->values[i].y);
        serialiser->serialise_int16(packet->values[i].z);
        // TODO: In future, optimise this, use the fact that the maximum value for a voxel is 254.
        // Make 255 a marker for: no more values that are not 0 or something
        serialiser->serialise_bytes(packet->values[i].voxel_values, CHUNK_EDGE_LENGTH * CHUNK_EDGE_LENGTH * CHUNK_EDGE_LENGTH);
    }
}

void n_deserialise_packet_chunk_voxels(
    packet_chunk_voxels_t *packet,
    serialiser_t *serialiser) {
    packet->chunk_in_packet_count = serialiser->deserialise_uint32();

    packet->values = LN_MALLOC(voxel_chunk_values_t, packet->chunk_in_packet_count);

    for (uint32_t i = 0; i < packet->chunk_in_packet_count; ++i) {
        packet->values[i].x = serialiser->deserialise_int16();
        packet->values[i].y = serialiser->deserialise_int16();
        packet->values[i].z = serialiser->deserialise_int16();

        packet->values[i].voxel_values = serialiser->deserialise_bytes(NULL, CHUNK_EDGE_LENGTH * CHUNK_EDGE_LENGTH * CHUNK_EDGE_LENGTH);
    }
}
