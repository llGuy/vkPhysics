#pragma once

#include <stdint.h>
#include <vkph_voxel.hpp>
#include <common/serialiser.hpp>

namespace vkph {

struct state_t;

}

namespace net {

constexpr uint32_t MAX_PREDICTED_CHUNK_MODIFICATIONS = 20;
constexpr uint32_t MAX_PREDICTED_VOXEL_MODIFICATIONS_PER_CHUNK = 250;

/*
  Represents the modification of just a SINGLE voxel.
 */
struct voxel_modification_t {
    uint16_t index;
    uint8_t final_value;

    /* 
      When the client sends this to the server, initial value will be stored
      When the server sends this to the client, color value will be stored
    */
    union {
        uint8_t initial_value;
        uint8_t color;
    };
};

/*
  Represents the modification of just a SINGLE chunk.
 */
struct chunk_modifications_t {
    int16_t x, y, z;
    uint32_t modified_voxels_count;

    /* 
      It's best to store modifications like so: colors and voxel_modification_t.
      This is because vkph::voxel_color_t is 1 byte, and voxel_modification_t is 4.
      If we stored the color with the voxel_modification_t, the entire structure
      will be padded to 6 bytes instead of 5. Storing it like this avoids this wasted
      memory.

      NOTE: Now that I think of it, the amount of memory saved really isn't dramatic.
    */
    voxel_modification_t modifications[MAX_PREDICTED_VOXEL_MODIFICATIONS_PER_CHUNK];
    vkph::voxel_color_t colors[MAX_PREDICTED_VOXEL_MODIFICATIONS_PER_CHUNK];

    /*
      Just some helper flags.
    */
    union {
        uint8_t flags;
        struct {
            uint8_t needs_to_correct: 1;
        };
    };
};

/*
  The first one fills in voxel_modification_t::initial_value whereas the second
  fills in voxel_modification_t::color (it's a union).
  
  Given the vkph::state_t's chunk history, these functions will fill in the passed in
  'chunk_modifications_t *modifications' structures.
*/
uint32_t fill_chunk_modification_array_with_initial_values(chunk_modifications_t *modifications, const vkph::state_t *state);
uint32_t fill_chunk_modification_array_with_colors(chunk_modifications_t *modifications, const vkph::state_t *state);

enum color_serialisation_type_t { CST_SERIALISE_UNION_COLOR = 0, CST_SERIALISE_SEPARATE_COLOR = 1 };

/* 
   color_serialisation_type_t parameter refers to whether to (de)serialise the color value from the colors array
   or to (de)serialise the color value from the union in the voxel_modification_t struct
*/
void serialise_chunk_modifications(
    chunk_modifications_t *modifications,
    uint32_t modification_count,
    serialiser_t *serialiser,
    color_serialisation_type_t);

chunk_modifications_t *deserialise_chunk_modifications(
    uint32_t *modification_count,
    serialiser_t *serialiser,
    color_serialisation_type_t);

void serialise_chunk_modification_meta_info(serialiser_t *, chunk_modifications_t *);
void serialise_chunk_modification_values_with_initial_values(serialiser_t *, chunk_modifications_t *);
void serialise_chunk_modification_colors_from_array(serialiser_t *, chunk_modifications_t *);
void deserialise_chunk_modification_meta_info(serialiser_t *, chunk_modifications_t *);
void deserialise_chunk_modification_values_with_initial_values(serialiser_t *, chunk_modifications_t *);
void deserialise_chunk_modification_colors_from_array(serialiser_t *, chunk_modifications_t *);

/*
  Any time there is "accumulated" in from of words linked to
  chunk modifications, it refers to all the chunk modifications
  (from multiple chunks, that is) that the client sent in a SINGLE
  client commands packet. It is important to remember that these
  packets get sent at a fixed interval (25 per second). If the server
  receives multiple of these before the game state dispatch, we "accumulate"
  the modifications of the chunks which were modified in both the
  new commands packet, and the ones in the old ones. This is so that
  we just store the modifications of a single individual chunk in one structure.
  If we just pushed every new chunk modification into the array of this struct,
  We would have so many chunk_modification_t's referring to modifications made
  in the SAME chunk and this is bad. We will hence "accumulate" these modifications
  so that we just store the modifications of each individual chunk in ONE struct.
*/
struct accumulated_predicted_modification_t {
    // Tick at which client sent these to the server
    uint64_t tick;
    // Accumulated predicted chunk modifications
    uint32_t acc_predicted_chunk_mod_count;
    // These will stay until server returns with game state dispatch confirming no errors have happened
    chunk_modifications_t *acc_predicted_modifications;
};

}
