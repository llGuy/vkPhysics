#include "net.hpp"
#include "wd_interp.hpp"

static chunks_to_interpolate_t chunks_to_interpolate;

void wd_interp_init() {
    chunks_to_interpolate.max_modified = 40;
    chunks_to_interpolate.modification_count = 0;
    chunks_to_interpolate.modifications = FL_MALLOC(chunk_modifications_t, chunks_to_interpolate.max_modified);
    memset(chunks_to_interpolate.modifications, 0, sizeof(chunk_modifications_t) * chunks_to_interpolate.max_modified);
}

void wd_chunks_interp_step(float dt) {
    chunks_to_interpolate.elapsed += dt;
    float progression = chunks_to_interpolate.elapsed / server_snapshot_interval();

    if (progression >= 1.0f) {
        progression = 1.0f;
    }
    
    for (uint32_t cm_index = 0; cm_index < chunks_to_interpolate.modification_count; ++cm_index) {
        chunk_modifications_t *cm_ptr = &chunks_to_interpolate.modifications[cm_index];
        chunk_t *c_ptr = get_chunk(ivector3_t(cm_ptr->x, cm_ptr->y, cm_ptr->z));

        c_ptr->flags.has_to_update_vertices = 1;
        for (uint32_t vm_index = 0; vm_index < cm_ptr->modified_voxels_count; ++vm_index) {
            voxel_modification_t *vm_ptr = &cm_ptr->modifications[vm_index];
            uint8_t *current_value = &c_ptr->voxels[vm_ptr->index];
            float fcurrent_value = (float)(*current_value);
            float initial_value = (float)(vm_ptr->initial_value);
            float final_value = (float)(vm_ptr->final_value);

            fcurrent_value = interpolate(initial_value, final_value, progression);
            if (fcurrent_value < 0.0f) {
                fcurrent_value = 0.0f;
            }
            else if (fcurrent_value > 254.0f) {
                fcurrent_value = 254.0f;
            }
            *current_value = (uint8_t)fcurrent_value;
        }
    }
}

chunks_to_interpolate_t *wd_get_chunks_to_interpolate() {
    return &chunks_to_interpolate;
}
