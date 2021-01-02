#pragma once

#include <stdint.h>

#include "net_chunk_tracker.hpp"
#include "net_game_client.hpp"
#include "net_game_server.hpp"

#include <containers.hpp>

namespace net {

constexpr uint32_t NET_MAX_CLIENT_COUNT = 50;
constexpr uint32_t NET_MAX_ACCUMULATED_PREDICTED_CHUNK_MODIFICATIONS_PACK_COUNT = 60;
constexpr uint32_t NET_MAX_ACCUMULATED_PREDICTED_CHUNK_MODIFICATIONS_PER_PACK = MAX_PREDICTED_CHUNK_MODIFICATIONS * 5;
constexpr uint32_t NET_MAX_MESSAGE_SIZE = 65507;
constexpr uint32_t NET_MAX_AVAILABLE_SERVER_COUNT = 1000;

/*
  All in seconds.
*/
constexpr float NET_CLIENT_COMMAND_OUTPUT_INTERVAL = (1.0f / 25.0f);
constexpr float NET_SERVER_SNAPSHOT_OUTPUT_INTERVAL = (1.0f / 20.0f);
constexpr float NET_SERVER_CHUNK_WORLD_OUTPUT_INTERVAL = (1.0f / 40.0f);
constexpr float NET_PING_INTERVAL = 2.0f;
constexpr float NET_CLIENT_TIMEOUT = 5.0f;

constexpr uint32_t GAME_OUTPUT_PORT_CLIENT = 6001;
constexpr uint32_t GAME_OUTPUT_PORT_SERVER = 6000;

constexpr uint32_t UNINITIALISED_TAG = 0xFFFFFFFF;

/*
  A lot of the members here need to be public simply because the client
  and server code need a lot of control over this data.
*/
struct context_t {

    /*
      Everytime something gets sent over the network, this value will get incrememted.
    */
    uint64_t current_packet;

    /*
      This could be either the client tag or the server tag.
    */
    uint32_t tag;

    /*
      Buffer allocated for creating messages.
    */
    char *message_buffer;

    /*
      All currently connected clients (will contain the exact same data on client and server
      - order, and everything...).
    */
    stack_container_t<client_t> clients;

    /*
      This is used as a temporary buffer when dealing with merging chunk modifications and
      taking care of chunk prediction errors.
    */
    uint8_t dummy_voxels[vkph::CHUNK_VOXEL_COUNT];

    /*
      This is where all the "accumulated" chunk modifications are stored (see net_chunk_tracker.hpp).
    */
    circular_buffer_array_t<
        accumulated_predicted_modification_t,
        NET_MAX_ACCUMULATED_PREDICTED_CHUNK_MODIFICATIONS_PACK_COUNT> accumulated_modifications;
    arena_allocator_t chunk_modification_allocator;
    accumulated_predicted_modification_t merged_recent_modifications;

    FILE *log_file;

    /*
      Methods:
    */
    void init_main_udp_socket(uint16_t output_post);
    bool main_udp_send_to(serialiser_t *serialiser, address_t address);
    int32_t main_udp_recv_from(char *message_buffer, uint32_t max_size, address_t *addr);
    void acc_predicted_modification_init(accumulated_predicted_modification_t *apm_ptr, uint64_t tick);
    accumulated_predicted_modification_t *add_acc_predicted_modification();
    void fill_dummy_voxels(chunk_modifications_t *modifications);
    void unfill_dummy_voxels(chunk_modifications_t *modifications);

    /*
      Creates a new accumulated_predicted_modifications of all the modfied chunks and voxels
      since last time vkph::state_t::reset_modification_tracker() was called.
    */
    accumulated_predicted_modification_t *accumulate_history(const vkph::state_t *state);

    /*
      'vkph::state_t *state' cannot be const because this function is going to flag all the modified
      chunks which requires modifying data in chunks.
      
      Combines while resolving conflicts, two chunk_modifications_t arrays.
      The result is stored in dst.
     */
    void merge_chunk_modifications(
        chunk_modifications_t *dst, uint32_t *dst_count,
        const chunk_modifications_t *src, uint32_t src_count,
        vkph::state_t *state);

private:

    socket_t main_udp_socket_;

};

}
