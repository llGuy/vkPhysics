#include "r_internal.hpp"

// For quads
struct quad_particle_prototype_t {
    float max_life;

    struct {
        uint8_t uses_texture: 1;
    } flags;

    // This is if the particle uses textures
    struct {
        uint32_t atlas_width;
        uint32_t atlas_height;
        uint32_t num_images;
    } texture;
};

// For lines
struct line_particle_prototype_t {
    
};
