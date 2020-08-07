#include <common/log.hpp>
#include <common/time.hpp>
#include <common/tools.hpp>
#include <common/string.hpp>
#include <common/socket.hpp>
#include <common/meta_packet.hpp>
#include <common/containers.hpp>
#include <common/allocators.hpp>
#include <common/serialiser.hpp>

static socket_t meta_socket;

#define MAX_CONNECTED_CLIENTS 10000
#define MAX_ACTIVE_SERVERS 500
#define MAX_PENDING_SOCKETS 100

#define RESPONSE_TIME 10.0f /* 20 Seconds */

#include <errno.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <chrono>

static void s_time_init() {
}

static time_stamp_t tick_start;

static void s_begin_time() {
    tick_start = current_time();
}

static time_stamp_t tick_end;

static float delta_time;

static void s_end_time() {
    tick_end = current_time();
    delta_time = time_difference(tick_end, tick_start);
}

static void s_meta_socket_init() {
    meta_socket = network_socket_init(SP_TCP);
    set_socket_to_non_blocking_mode(meta_socket);

    int32_t value = 1;
    setsockopt(meta_socket, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(int32_t));

    network_address_t address = {};
    address.port = host_to_network_byte_order(SERVER_META_OUTPUT_PORT);

    bind_network_socket_to_port(meta_socket, address);
    set_socket_to_listening(meta_socket, MAX_ACTIVE_SERVERS + MAX_CONNECTED_CLIENTS);
}

struct connection_t {
    union {
        struct {
            uint32_t initialised: 1;
            uint32_t responded: 1;
        } flags;

        uint32_t bytes;
    };
    
    socket_t sock;
    network_address_t address;

    time_stamp_t time_stamp;
};

// Sockets for which we don't know if they are clients or servers
static stack_container_t<connection_t> pending_sockets;

// Maps socket_t to connection_t in pending sockets
//static hash_table_t<uint32_t, 100, 5, 5> socket_to_connection;

struct game_client_t {
    connection_t connection;
    // Client information
    const char *client_name;
};

static stack_container_t<game_client_t> client_sockets;
static hash_table_t<uint32_t, 100, 5, 5> client_name_to_socket;

struct game_server_t {
    connection_t connection;
    // Server information
    const char *server_name;
    uint32_t max_clients;
    uint32_t client_count;
};

static stack_container_t<game_server_t> server_sockets;

static void s_client_and_server_sockets_init() {
    client_sockets.init(MAX_CONNECTED_CLIENTS);
    server_sockets.init(MAX_ACTIVE_SERVERS);
    pending_sockets.init(MAX_PENDING_SOCKETS);

    uint32_t i = 0;
    for (; i < MAX_ACTIVE_SERVERS; ++i) {
        client_sockets[i].connection.bytes = 0;
        client_sockets[i].connection.sock = -1;
        client_sockets[i].connection.address = {};

        server_sockets[i].connection.bytes = 0;
        server_sockets[i].connection.sock = -1;
        server_sockets[i].connection.address = {};
    }

    for(; i < MAX_CONNECTED_CLIENTS; ++i) {
        client_sockets[i].connection.bytes = 0;
        client_sockets[i].connection.sock = -1;
        client_sockets[i].connection.address = {};
    }
}

static void s_check_new_connections() {
    accepted_connection_t new_connection = accept_connection(meta_socket);

    if (new_connection.s >= 0) {
        LOG_INFO("Received connection request, pushing to pending list\n");

        uint32_t index = pending_sockets.add();
        //socket_to_connection.insert(new_connection.s, index);
        pending_sockets[index].flags.initialised = 1;// = {1, new_connection.s, new_connection.address};
        pending_sockets[index].sock = new_connection.s;
        pending_sockets[index].address = new_connection.address;
        pending_sockets[index].flags.responded = 1;

        // Set socket to be in non blocking mode
        set_socket_to_non_blocking_mode(pending_sockets[index].sock);
    }
    else {
        // Didn't actually receive any requests from any game servers
    }
}

#define MAX_MESSAGE_SIZE 10000

static uint8_t *message_buffer;

static bool need_to_send_available_servers;

static void s_check_pending_sockets() {
    // First check pending sockets
    for (uint32_t i = 0; i < pending_sockets.data_count; ++i) {
        connection_t *pending = &pending_sockets[i];
        if (pending->flags.initialised) {
            int32_t byte_count = receive_from_bound_address(pending->sock, (char *)message_buffer, MAX_MESSAGE_SIZE);

            // Packet can only be register packet
            if (byte_count) {
                serialiser_t serialiser = {};
                serialiser.data_buffer = message_buffer;
                serialiser.data_buffer_size = byte_count;

                meta_packet_header_t header = {};
                deserialise_meta_packet_header(&header, &serialiser);

                if (header.type == HPT_QUERY_SERVER_REGISTER) {
                    uint32_t index = server_sockets.add();
                    server_sockets[index].connection = *pending;
                    server_sockets[index].connection.flags.responded = 1;
                    
                    // Register socket as server
                    pending_sockets.remove(i);

                    meta_query_server_register_t query;
                    deserialise_meta_query_server_register(&query, &serialiser);
                    server_sockets[index].server_name = create_fl_string(query.server_name);
                    server_sockets[index].max_clients = query.max_clients;
                    server_sockets[index].client_count = query.client_count;
                    server_sockets[index].connection.flags.initialised = 1;

                    LOG_INFOV("New server active: %s\n", server_sockets[index].server_name);

                    need_to_send_available_servers = 1;
                }
                else if (header.type == HPT_QUERY_CLIENT_REGISTER) {
                    meta_query_client_register_t query;
                    //deserialise_meta_query_client_register(&query, &serialiser);
                    query.client_name = serialiser.deserialise_string();
                    
                    uint32_t *existing = client_name_to_socket.get(simple_string_hash(query.client_name));
                    
                    if (existing) {
                        // Client already exists
                        LOG_INFOV("Client %s already exists, not creating new client\n", query.client_name);

                        // Check if the IP address is the same
                        game_client_t *existing_client = &client_sockets[*existing];
                        if (existing_client->connection.address.ipv4_address == pending->address.ipv4_address) {
                            LOG_INFOV("Client %s has same address as existing client, not creating new client\n", query.client_name);
                            existing_client->connection = *pending;
                            existing_client->connection.flags.responded = 1;
                            existing_client->connection.flags.initialised = 1;
                        }
                        // TODO: Resolve naming conflicts
                        /*else {
                            uint32_t index = client_sockets.add();
                            client_sockets[index].connection = *pending;
                            client_sockets[index].connection.flags.responded = 1;
                    
                            client_sockets[index].client_name = create_fl_string(query.client_name);
                            client_sockets[index].connection.flags.initialised = 1;

                            LOG_INFOV("New client active: %s\n", client_sockets[index].client_name);
                        
                            client_name_to_socket.insert(simple_string_hash(query.client_name), index);
                            }*/
                    }
                    else {
                        // Register socket as client
                        uint32_t index = client_sockets.add();
                        client_sockets[index].connection = *pending;
                        client_sockets[index].connection.flags.responded = 1;
                    
                        client_sockets[index].client_name = create_fl_string(query.client_name);
                        client_sockets[index].connection.flags.initialised = 1;

                        LOG_INFOV("New client active: %s\n", client_sockets[index].client_name);
                        
                        client_name_to_socket.insert(simple_string_hash(query.client_name), index);
                    }
                    
                    pending_sockets.remove(i);
                }
                else if (header.type == HPT_RESPONSE_RESPONSIVENESS) {
                    pending->flags.responded = 1;
                }
                else {
                    // There was an error, was not supposed to receive this packet before any other from this socket
                    LOG_ERROR("There was an error: received non register packet as first packet\n");
                }
            }
        }
    }
}

static void s_handle_query_available_servers(
    serialiser_t *in_serialiser,
    serialiser_t *out_serialiser,
    game_client_t *client) {
    (void)in_serialiser;
    meta_packet_header_t header = {};
    header.type = HPT_RESPONSE_AVAILABLE_SERVERS;
    serialise_meta_packet_header(&header, out_serialiser);
    // Need to send all available servers back to the client
    uint32_t server_count = server_sockets.data_count - server_sockets.removed_count;
    out_serialiser->serialise_uint32(server_count);
    for (uint32_t s_index = 0; s_index < server_sockets.data_count; ++s_index) {
        game_server_t *server = &server_sockets[s_index];
        if (server->connection.flags.initialised) {
            meta_server_info_t server_info = {};
            server_info.server_name = server->server_name;
            server_info.ipv4_address = server->connection.address.ipv4_address;
            server_info.max_clients = server->max_clients;
            server_info.client_count = server->client_count;
            serialise_meta_server_info(&server_info, out_serialiser);
        }
    }

    send_to_bound_address(client->connection.sock, (char *)out_serialiser->data_buffer, out_serialiser->data_buffer_head);
}

static void s_process_client_packet(
    serialiser_t *in_serialiser,
    serialiser_t *out_serialiser,
    game_client_t *client) {
    meta_packet_header_t header = {};

    deserialise_meta_packet_header(&header, in_serialiser);

    switch (header.type) {
    case HPT_QUERY_AVAILABLE_SERVERS: {
        LOG_INFO("Client requested available servers\n");
        s_handle_query_available_servers(in_serialiser, out_serialiser, client);
        break;
    }
    case HPT_RESPONSE_RESPONSIVENESS: {
        client->connection.flags.responded = 1;
        break;
    }
    default: {
    }
    }
}

static void s_process_server_packet(
    serialiser_t *in_serialiser,
    serialiser_t *out_serialiser,
    game_server_t *server) {
    meta_packet_header_t header = {};

    deserialise_meta_packet_header(&header, in_serialiser);

    switch (header.type) {
    case HPT_RESPONSE_RESPONSIVENESS: {
        server->connection.flags.responded = 1;
        break;
    }
    default: {
    }
    }
}

static void s_check_queries() {
    serialiser_t out_serialiser = {};
    out_serialiser.init(10000);
    for (uint32_t i = 0; i < client_sockets.data_count; ++i) {
        game_client_t *gc_ptr = &client_sockets[i];
        
        if (gc_ptr->connection.flags.initialised) {
            if (need_to_send_available_servers) {
                s_handle_query_available_servers(NULL, &out_serialiser, gc_ptr);
                out_serialiser.data_buffer_head = 0;
            }
            
            int32_t bytes_received = receive_from_bound_address(gc_ptr->connection.sock, (char *)message_buffer, MAX_MESSAGE_SIZE);

            if (bytes_received > 0) {
                // Process packet
                serialiser_t serialiser;
                serialiser.data_buffer = message_buffer;
                serialiser.data_buffer_size = bytes_received;
                s_process_client_packet(&serialiser, &out_serialiser, gc_ptr);

                out_serialiser.data_buffer_head = 0;
            }
        }
    }

    need_to_send_available_servers = 0;
    
    for (uint32_t i = 0; i < server_sockets.data_count; ++i) {
        game_server_t *gs_ptr = &server_sockets[i];
        if (gs_ptr->connection.flags.initialised) {
            int32_t bytes_received = receive_from_bound_address(gs_ptr->connection.sock, (char *)message_buffer, MAX_MESSAGE_SIZE);

            if (bytes_received > 0) {
                serialiser_t serialiser;
                serialiser.data_buffer = message_buffer;
                serialiser.data_buffer_size = bytes_received;
                s_process_server_packet(&serialiser, &out_serialiser, gs_ptr);

                out_serialiser.data_buffer_head = 0;
            }
        }
    }
}

static time_stamp_t last_responsiveness_query;

static void s_check_new_queries() {
    s_check_pending_sockets();
    s_check_queries();

    // Make sure to check every so often if servers / clients are still active
    time_stamp_t current = current_time();
    float dt = time_difference(current, last_responsiveness_query); 

    if (dt > RESPONSE_TIME) {
        serialiser_t serialiser = {};
        serialiser.init(20);

        meta_packet_header_t header = {};
        header.type = HPT_QUERY_RESPONSIVENESS;

        serialise_meta_packet_header(&header, &serialiser);

        for (uint32_t i = 0; i < pending_sockets.data_count; ++i) {
            connection_t *ps_ptr = &pending_sockets[i];
            
            if (ps_ptr->flags.initialised) {
                if (!ps_ptr->flags.responded) {
                    // Disconnect client
                    LOG_INFO("Pending socket is not responding anymore, removing\n");
                    pending_sockets.remove(i);
                }
                else {
                    ps_ptr->time_stamp = current;
                    ps_ptr->flags.responded = 0;

                    // Send to packet to client asking for response
                    if (!send_to_bound_address(ps_ptr->sock, (char *)serialiser.data_buffer, serialiser.data_buffer_head)) {
                        // Pipe broke, need to remove client
                        LOG_INFO("Pending socket is not responding anymore, removing\n");
                        pending_sockets.remove(i);
                    }
                }
            }
        }
        
        for (uint32_t i = 0; i < client_sockets.data_count; ++i) {
            game_client_t *gc_ptr = &client_sockets[i];
            
            if (gc_ptr->connection.flags.initialised) {
                if (!gc_ptr->connection.flags.responded) {
                    // Disconnect client
                    LOG_INFOV("Client %s is not responding anymore, removing\n", gc_ptr->client_name);
                    client_name_to_socket.remove(simple_string_hash(gc_ptr->client_name));
                    client_sockets.remove(i);
                }
                else {
                    gc_ptr->connection.time_stamp = current;
                    gc_ptr->connection.flags.responded = 0;

                    // Send to packet to client asking for response
                    if (!send_to_bound_address(gc_ptr->connection.sock, (char *)serialiser.data_buffer, serialiser.data_buffer_head)) {
                        // Pipe broke, need to remove client
                        LOG_INFOV("Client %s is not responding anymore, removing\n", gc_ptr->client_name);
                        client_name_to_socket.remove(simple_string_hash(gc_ptr->client_name));
                        client_sockets.remove(i);
                    }
                }
            }
        }

        for (uint32_t i = 0; i < server_sockets.data_count; ++i) {
            game_server_t *gs_ptr = &server_sockets[i];

            if (gs_ptr->connection.flags.initialised) {
                if (!gs_ptr->connection.flags.responded) {
                    // Disconnect client
                    LOG_INFOV("Server %s is not responding anymore, removing\n", gs_ptr->server_name);
                    server_sockets.remove(i);

                    need_to_send_available_servers = 1;
                }
                else {
                    gs_ptr->connection.time_stamp = current;
                    gs_ptr->connection.flags.responded = 0;

                    // Send to packet to client asking for response
                    if (!send_to_bound_address(gs_ptr->connection.sock, (char *)serialiser.data_buffer, serialiser.data_buffer_head)) {
                        // Pipe broke, need to remove client
                        LOG_INFOV("Server %s is not responding anymore, removing\n", gs_ptr->server_name);
                        server_sockets.remove(i);
                        
                        need_to_send_available_servers = 1;
                    }
                }
            }
        }

        last_responsiveness_query = current;
    }
}

static void s_start_loop() {
    bool running = 1;
    last_responsiveness_query = current_time();
    while(running) {
        s_begin_time();
        s_check_new_connections();
        s_check_new_queries();
        LN_CLEAR();

        sleep_seconds(0.015f);
        
        s_end_time();
    }
}

#include <signal.h>

void sig_handler(int32_t s) {
    // Close socket
    destroy_socket(meta_socket);
    exit(0);
}

int32_t main(
    int32_t argc,
    char *argv[]) {
    client_name_to_socket.init();

    struct sigaction handler;
    handler.sa_handler = sig_handler;
    sigemptyset(&handler.sa_mask);
    handler.sa_flags = 0;

    sigaction(SIGINT, &handler, NULL);
    
    message_buffer = FL_MALLOC(uint8_t, MAX_MESSAGE_SIZE);

    global_linear_allocator_init((uint32_t)megabytes(10));
    //socket_to_connection.init();

    socket_api_init();
    s_meta_socket_init();
    s_client_and_server_sockets_init();
    s_start_loop();
}
