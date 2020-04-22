#pragma once

#include "tools.hpp"
#include "serialiser.hpp"

enum hub_packet_type_t : uint32_t {
    HPT_QUERY_SERVER_REGISTER,
    HPT_QUERY_CLIENT_REGISTER,
    
    HPT_QUERY_AVAILABLE_SERVERS,
    HPT_RESPONSE_AVAILABLE_SERVERS,

    HPT_INFORM_CLIENT_COUNT,

    HPT_QUERY_RESPONSIVENESS,
    HPT_RESPONSE_RESPONSIVENESS    
};

struct hub_packet_header_t {
    hub_packet_type_t type;
};

void serialise_hub_packet_header(
    hub_packet_header_t *header,
    serialiser_t *serialiser);

void deserialise_hub_packet_header(
    hub_packet_header_t *header,
    serialiser_t *serialiser);

struct hub_query_server_register_t {
    const char *server_name;

    uint32_t max_clients;
    uint32_t client_count;
};

void serialise_hub_query_server_register(
    hub_query_server_register_t *query,
    serialiser_t *serialiser);

void deserialise_hub_query_server_register(
    hub_query_server_register_t *query,
    serialiser_t *serialiser);

struct hub_query_client_register_t {
    const char *client_name;
};

void serialise_hub_query_client_register(
    hub_query_client_register_t *query,
    serialiser_t *serialiser);

void deserialise_hub_query_client_register(
    hub_query_client_register_t *query,
    serialiser_t *serialiser);

struct hub_query_available_servers_t {
    // In future, have filters
};

struct hub_server_info_t {
    const char *server_name;
    uint32_t ipv4_address;
    uint32_t max_clients;
    uint32_t client_count;
};

void serialise_hub_server_info(
    hub_server_info_t *info,
    serialiser_t *serialiser);

void deserialise_hub_server_info(
    hub_server_info_t *info,
    serialiser_t *serialiser);

struct hub_response_available_servers_t {
    uint32_t server_count;
    hub_server_info_t *servers;
};

void serialise_hub_response_available_servers(
    hub_response_available_servers_t *response,
    serialiser_t *serialiser);

void deserialise_hub_response_available_servers(
    hub_response_available_servers_t *response,
    serialiser_t *serialiser);
