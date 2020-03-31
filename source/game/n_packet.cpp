#include "n_internal.hpp"

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

constexpr uint32_t n_packed_connection_handshake_size() {
    return
        sizeof(packet_connection_handshake_t::client_id);
}

void n_serialise_packet_header(
    packet_header_t *header,
    serialiser_t *serialiser) {
    serialiser->serialise_uint32(header->flags.bytes);
    serialiser->serialise_uint64(header->current_tick);
    serialiser->serialise_uint64(header->current_packet_count);
    serialiser->serialise_uint32(header->client_id);
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
    
}

void n_serialise_connection_handshake(
    packet_connection_handshake_t *packet,
    serialiser_t *serialiser) {
    serialiser->serialise_uint16(packet->client_id);
}

void n_deserialise_connection_handshake(
    packet_connection_handshake_t *packet,
    serialiser_t *serialiser) {
    packet->client_id = serialiser->deserialise_uint16();
}
