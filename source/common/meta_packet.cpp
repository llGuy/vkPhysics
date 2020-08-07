#include "meta_packet.hpp"
#include "allocators.hpp"

void serialise_meta_packet_header(
    meta_packet_header_t *header,
    serialiser_t *serialiser) {
    serialiser->serialise_uint32(header->type);
}

void deserialise_meta_packet_header(
    meta_packet_header_t *header,
    serialiser_t *serialiser) {
    header->type = (meta_packet_type_t)serialiser->deserialise_uint32();
}

void serialise_meta_query_server_register(
    meta_query_server_register_t *query,
    serialiser_t *serialiser) {
    serialiser->serialise_string(query->server_name);
    serialiser->serialise_uint32(query->max_clients);
    serialiser->serialise_uint32(query->client_count);
}

void deserialise_meta_query_server_register(
    meta_query_server_register_t *query,
    serialiser_t *serialiser) {
    query->server_name = serialiser->deserialise_string();
    query->max_clients = serialiser->deserialise_uint32();
    query->client_count = serialiser->deserialise_uint32();
}

void serialise_meta_query_client_register(
    meta_query_client_register_t *query,
    serialiser_t *serialiser) {
    serialiser->serialise_string(query->client_name);
}

void deserialise_meta_query_client_register(
    meta_query_client_register_t *query,
    serialiser_t *serialiser) {
    query->client_name- serialiser->deserialise_string();
}

void serialise_meta_server_info(
    meta_server_info_t *info,
    serialiser_t *serialiser) {
    serialiser->serialise_string(info->server_name);
    serialiser->serialise_uint32(info->ipv4_address);
    serialiser->serialise_uint32(info->max_clients);
    serialiser->serialise_uint32(info->client_count);
}

void deserialise_meta_server_info(
    meta_server_info_t *info,
    serialiser_t *serialiser) {
    info->server_name = serialiser->deserialise_string();
    info->ipv4_address = serialiser->deserialise_uint32();
    info->max_clients = serialiser->deserialise_uint32();
    info->client_count = serialiser->deserialise_uint32();
}

void serialise_meta_response_available_servers(
    meta_response_available_servers_t *response,
    serialiser_t *serialiser) {
    serialiser->serialise_uint32(response->server_count);
    for (uint32_t i = 0; i < response->server_count; ++i) {
        serialise_meta_server_info(&response->servers[i], serialiser);
    }
}

void deserialise_meta_response_available_servers(
    meta_response_available_servers_t *response,
    serialiser_t *serialiser) {
    response->server_count = serialiser->deserialise_uint32();
    response->servers = LN_MALLOC(meta_server_info_t, response->server_count);
    for (uint32_t i = 0; i < response->server_count; ++i) {
        deserialise_meta_server_info(&response->servers[i], serialiser);
    }
}
