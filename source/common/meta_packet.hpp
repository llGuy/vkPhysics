#pragma once

#include "tools.hpp"
#include "serialiser.hpp"

enum meta_packet_type_t : uint32_t {
    HPT_QUERY_SERVER_REGISTER,
    HPT_QUERY_CLIENT_REGISTER,
    
    HPT_QUERY_AVAILABLE_SERVERS,
    HPT_RESPONSE_AVAILABLE_SERVERS,

    HPT_INFORM_CLIENT_COUNT,

    HPT_QUERY_RESPONSIVENESS,
    HPT_RESPONSE_RESPONSIVENESS    
};

struct meta_packet_header_t {
    meta_packet_type_t type;
};

void serialise_meta_packet_header(
    meta_packet_header_t *header,
    serialiser_t *serialiser);

void deserialise_meta_packet_header(
    meta_packet_header_t *header,
    serialiser_t *serialiser);

struct meta_query_server_register_t {
    const char *server_name;

    uint32_t max_clients;
    uint32_t client_count;
};

void serialise_meta_query_server_register(
    meta_query_server_register_t *query,
    serialiser_t *serialiser);

void deserialise_meta_query_server_register(
    meta_query_server_register_t *query,
    serialiser_t *serialiser);

struct meta_query_client_register_t {
    const char *client_name;
};

void serialise_meta_query_client_register(
    meta_query_client_register_t *query,
    serialiser_t *serialiser);

void deserialise_meta_query_client_register(
    meta_query_client_register_t *query,
    serialiser_t *serialiser);

struct meta_query_available_servers_t {
    // In future, have filters
};

struct meta_server_info_t {
    const char *server_name;
    uint32_t ipv4_address;
    uint32_t max_clients;
    uint32_t client_count;
};

void serialise_meta_server_info(
    meta_server_info_t *info,
    serialiser_t *serialiser);

void deserialise_meta_server_info(
    meta_server_info_t *info,
    serialiser_t *serialiser);

struct meta_response_available_servers_t {
    uint32_t server_count;
    meta_server_info_t *servers;
};

void serialise_meta_response_available_servers(
    meta_response_available_servers_t *response,
    serialiser_t *serialiser);

void deserialise_meta_response_available_servers(
    meta_response_available_servers_t *response,
    serialiser_t *serialiser);
