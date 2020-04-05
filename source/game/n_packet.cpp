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
    return sizeof(uint8_t) * strlen(connection_request->name);
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
    serialiser->serialise_uint32(full_game_state->player_count);
    for (uint32_t i = 0; i < full_game_state->player_count; ++i) {
        serialiser->serialise_string(full_game_state->player_infos[i].name);
        serialiser->serialise_uint16(full_game_state->player_infos[i].client_id);
        serialiser->serialise_vector3(full_game_state->player_infos[i].ws_position);
        serialiser->serialise_vector3(full_game_state->player_infos[i].ws_view_direction);
        serialiser->serialise_vector3(full_game_state->player_infos[i].ws_up_vector);
        serialiser->serialise_float32(full_game_state->player_infos[i].default_speed);
        serialiser->serialise_uint8(full_game_state->player_infos[i].is_local);
    }
}

void n_deserialise_connection_handshake(
    packet_connection_handshake_t *full_game_state,
    serialiser_t *serialiser) {
    full_game_state->player_count = serialiser->deserialise_uint32();
    full_game_state->player_infos = LN_MALLOC(full_player_info_t, full_game_state->player_count);

    for (uint32_t i = 0; i < full_game_state->player_count; ++i) {
        full_game_state->player_infos[i].name = serialiser->deserialise_string();
        full_game_state->player_infos[i].client_id = serialiser->deserialise_uint16();
        full_game_state->player_infos[i].ws_position = serialiser->deserialise_vector3();
        full_game_state->player_infos[i].ws_view_direction = serialiser->deserialise_vector3();
        full_game_state->player_infos[i].ws_up_vector = serialiser->deserialise_vector3();
        full_game_state->player_infos[i].default_speed = serialiser->deserialise_float32();
        full_game_state->player_infos[i].is_local = serialiser->deserialise_uint8();
    }
}

uint32_t n_packed_player_joined_size(
    packet_player_joined_t *packet) {
    uint32_t total_size = 0;
    total_size += strlen(packet->player_info.name);
    total_size += sizeof(full_player_info_t::client_id);
    total_size += sizeof(full_player_info_t::ws_position);
    total_size += sizeof(full_player_info_t::ws_view_direction);
    total_size += sizeof(full_player_info_t::ws_up_vector);
    total_size += sizeof(full_player_info_t::default_speed);
    total_size += sizeof(full_player_info_t::is_local);

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
    serialiser->serialise_uint8(packet->player_info.is_local);
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
    packet->player_info.is_local = serialiser->deserialise_uint8();
}
