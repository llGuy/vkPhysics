#pragma once

#include <common/tools.hpp>
#include <common/event.hpp>
#include <common/socket.hpp>
#include <common/player.hpp>
#include <common/containers.hpp>

#define NET_DEBUG 0
#define NET_DEBUG_LAG 0
#define NET_DEBUG_VOXEL_INTERPOLATION 0

void net_init(event_submissions_t *events);

struct local_client_info_t {
    const char *name;
};

void tick_net(event_submissions_t *events);

bool connected_to_server();
float server_snapshot_interval();
float client_command_interval();

struct voxel_chunk_values_t {
    // Chunk coord
    int16_t x, y, z;
    uint8_t *voxel_values;
};

struct packet_chunk_voxels_t {
    uint32_t chunk_in_packet_count;
    voxel_chunk_values_t *values;
};

// BECAUSE THERE ISN'T A UI SYSTEM IN PLACE YET

uint16_t get_local_client_index();

available_servers_t *get_available_servers();
