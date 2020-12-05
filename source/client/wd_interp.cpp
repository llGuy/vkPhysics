#include "wd_interp.hpp"
#include <common/game.hpp>
#include "common/constant.hpp"
#include "common/player.hpp"
#include <common/net.hpp>

static chunks_to_interpolate_t chunks_to_interpolate;

void wd_interp_init() {
    chunks_to_interpolate.max_modified = 40;
    chunks_to_interpolate.modification_count = 0;
    chunks_to_interpolate.modifications = FL_MALLOC(chunk_modifications_t, chunks_to_interpolate.max_modified);
    memset(chunks_to_interpolate.modifications, 0, sizeof(chunk_modifications_t) * chunks_to_interpolate.max_modified);
}

void wd_finish_interp_step() {
    chunks_to_interpolate.elapsed = 0.0f;

    for (uint32_t cm_index = 0; cm_index < chunks_to_interpolate.modification_count; ++cm_index) {
        chunk_modifications_t *cm_ptr = &chunks_to_interpolate.modifications[cm_index];
        chunk_t *c_ptr = g_game->get_chunk(ivector3_t(cm_ptr->x, cm_ptr->y, cm_ptr->z));

        c_ptr->flags.has_to_update_vertices = 1;
        for (uint32_t vm_index = 0; vm_index < cm_ptr->modified_voxels_count; ++vm_index) {
            voxel_modification_t *vm_ptr = &cm_ptr->modifications[vm_index];
            voxel_t *current_value = &c_ptr->voxels[vm_ptr->index];
            current_value->value = vm_ptr->final_value;
        }

        cm_ptr->modified_voxels_count = 0;
    }

    chunks_to_interpolate.modification_count = 0;
}

void wd_chunks_interp_step(float dt) {
    chunks_to_interpolate.elapsed += dt;
    float progression = chunks_to_interpolate.elapsed / NET_SERVER_SNAPSHOT_OUTPUT_INTERVAL;

    if (progression >= 1.0f) {
        progression = 1.0f;
    }
    
    for (uint32_t cm_index = 0; cm_index < chunks_to_interpolate.modification_count; ++cm_index) {
        chunk_modifications_t *cm_ptr = &chunks_to_interpolate.modifications[cm_index];
        chunk_t *c_ptr = g_game->get_chunk(ivector3_t(cm_ptr->x, cm_ptr->y, cm_ptr->z));

        c_ptr->flags.has_to_update_vertices = 1;
        for (uint32_t vm_index = 0; vm_index < cm_ptr->modified_voxels_count; ++vm_index) {
            voxel_modification_t *vm_ptr = &cm_ptr->modifications[vm_index];
            voxel_t *current_value = &c_ptr->voxels[vm_ptr->index];
            float fcurrent_value = (float)(current_value->value);
            float initial_value = (float)(vm_ptr->initial_value);
            float final_value = (float)(vm_ptr->final_value);

            fcurrent_value = interpolate(initial_value, final_value, progression);
            if (fcurrent_value < 0.0f) {
                fcurrent_value = 0.0f;
            }
            else if (fcurrent_value > 254.0f) {
                fcurrent_value = 254.0f;
            }
            current_value->value = (uint8_t)fcurrent_value;
        }
    }
}

void wd_player_interp_step(
    float dt,
    player_t *p) {
    // This adds a little delay
    // Makes sure that there is always snapshots to interpolate between
    if (p->remote_snapshots.head_tail_difference >= 3) {
        uint32_t previous_snapshot_index = p->remote_snapshots.tail;
        uint32_t next_snapshot_index = p->remote_snapshots.tail;

        if (++next_snapshot_index == p->remote_snapshots.buffer_size) {
            next_snapshot_index = 0;
        }

        p->elapsed += dt;

        float progression = p->elapsed / NET_SERVER_SNAPSHOT_OUTPUT_INTERVAL;

        // It is possible that progression went way past maximum (in case of extreme lag, so need to
        // take into account how many times over maximum time we went)
        if (progression >= 1.0f) {
            int32_t skip_count = (int32_t)(floor(progression));
            //progression = fmod(progression, 1.0f);
            progression -= (float)skip_count;
            p->elapsed = progression * NET_SERVER_SNAPSHOT_OUTPUT_INTERVAL;
            // p->elapsed -= NET_SERVER_SNAPSHOT_OUTPUT_INTERVAL * (float)skip_count;

            for (int32_t i = 0; i < skip_count; ++i) {
                p->remote_snapshots.get_next_item_tail();
            }

            previous_snapshot_index = p->remote_snapshots.tail;
            next_snapshot_index = p->remote_snapshots.tail;

            if (++next_snapshot_index == p->remote_snapshots.buffer_size) {
                next_snapshot_index = 0;
            }
        }

        p->snapshot_before = previous_snapshot_index;
        p->snapshot_after = next_snapshot_index;

        player_snapshot_t *previous_snapshot = &p->remote_snapshots.buffer[previous_snapshot_index];
        player_snapshot_t *next_snapshot = &p->remote_snapshots.buffer[next_snapshot_index];

        // For things that cannot be interpolated
        player_snapshot_t *middle_snapshot = previous_snapshot;
        if (progression > 0.5f) {
            middle_snapshot = next_snapshot;
        }

        vector3_t previous_position = p->ws_position;
        p->ws_position = interpolate(previous_snapshot->ws_position, next_snapshot->ws_position, progression);
        p->ws_view_direction = interpolate(previous_snapshot->ws_view_direction, next_snapshot->ws_view_direction, progression);
        p->ws_up_vector = interpolate(previous_snapshot->ws_up_vector, next_snapshot->ws_up_vector, progression);

        // Just so that it's not zero
        p->ws_velocity = p->ws_position - previous_position;
        p->flags.contact = middle_snapshot->contact;

        bool switch_shapes = (p->flags.interaction_mode != middle_snapshot->interaction_mode);

        if (switch_shapes) {
            LOG_INFOV("Switching shapes: %d to %d\n", p->flags.interaction_mode, middle_snapshot->interaction_mode);
        }

        handle_shape_switch(p, switch_shapes, dt);
        p->flags.interaction_mode = middle_snapshot->interaction_mode;

        p->animated_state = (player_animated_state_t)middle_snapshot->animated_state;

        if (p->flags.contact == PCS_ON_GROUND) {
            p->frame_displacement = glm::length(previous_position - p->ws_position);
        }

        p->flags.alive_state = middle_snapshot->alive_state;

        update_player_chunk_status(p);
    }
}

chunks_to_interpolate_t *wd_get_chunks_to_interpolate() {
    return &chunks_to_interpolate;
}
