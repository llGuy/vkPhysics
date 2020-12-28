#pragma once

#include <common/containers.hpp>
#include "tools.hpp"
#include "socket.hpp"
#include "constant.hpp"
#include "serialiser.hpp"

#include <vkph_weapon.hpp>
#include <vkph_player.hpp>
#include <vkph_projectile.hpp>


void main_udp_socket_init(uint16_t output_port);
void meta_socket_init();

bool send_to_game_server(serialiser_t *serialiser, network_address_t address);
int32_t receive_from_game_server(char *message_buffer, uint32_t max_size, network_address_t *addr);
int32_t receive_from_client(char *message_buffer, uint32_t max_size, network_address_t *addr);
bool send_to_meta_server(serialiser_t *serialiser);
bool send_to_client(serialiser_t *serialiser, network_address_t address);
void acc_predicted_modification_init(accumulated_predicted_modification_t *apm_ptr, uint64_t tick);
accumulated_predicted_modification_t *add_acc_predicted_modification();
void check_incoming_meta_server_packets();
void flag_modified_chunks(chunk_modifications_t *modifications, uint32_t count, vkph::state_t *state);
void unflag_modified_chunks(chunk_modifications_t *modifications, uint32_t count, vkph::state_t *state);
void fill_dummy_voxels(chunk_modifications_t *modifications);
void unfill_dummy_voxels(chunk_modifications_t *modifications);
uint32_t fill_chunk_modification_array_with_initial_values(chunk_modifications_t *modifications, vkph::state_t *state);
uint32_t fill_chunk_modification_array_with_colors(chunk_modifications_t *modifications, vkph::state_t *state);
accumulated_predicted_modification_t *accumulate_history(vkph::state_t *state);
void merge_chunk_modifications(
    chunk_modifications_t *dst,
    uint32_t *dst_count,
    chunk_modifications_t *src,
    uint32_t src_count,
    vkph::state_t *state);

