#include "ai.hpp"
#include "common/player.hpp"
#include "w_internal.hpp"
#include <common/time.hpp>
#include <common/event.hpp>
#include <common/containers.hpp>

static world_ai_t ai;

static bool s_reset_arena(
    ai_training_session_t session_type) {
#if 0
    bool evolved = 0;

    if (ai.iteration_count == ai.population_count) {
        // Need to reset evolve
        ai.iteration_count = 0;

        evolve_population();

        evolved = 1;
    }

    w_clear_players_and_render_rsc();
    w_clear_chunks_and_render_rsc();

    switch (session_type) {

    case ATS_WALKING: {
        // Just initialise one player
        player_init_info_t info = {};
        info.ws_position = vector3_t(0.0f, 1.0f, 0.0f);
        info.ws_view_direction = vector3_t(0.0f, 0.0f, -1.0f);
        info.ws_up_vector = vector3_t(0.0f, 1.0f, 0.0f);
        info.default_speed = 15.0f;

        player_flags_t flags = {};
        flags.u32 = 0;
        flags.alive_state = PAS_ALIVE;
        flags.interaction_mode = PIM_STANDING;

        info.flags = flags.u32;

        player_t *p = add_player();
        fill_player_info(p, &info);

        if (ai.first_iteration) {
            uint32_t ai_id = attach_ai(p->local_id);
            p->ai_id = ai_id;
        }
        else {
            iterate_ai(p->ai_id, ai.iteration_count);
        }

        p->ai = 1;
        p->calculated_score = 1;
        p->ai_start_time = 0.0f;

        info.ws_position = vector3_t(5.0f, 0.0f, 5.0f);
        info.ws_view_direction = vector3_t(0.0f, 0.0f, -1.0f);
        info.ws_up_vector = vector3_t(0.0f, 1.0f, 0.0f);
        info.default_speed = 0.0f;

        flags.u32 = 0;
        flags.alive_state = PAS_ALIVE;
        flags.interaction_mode = PIM_BALL;

        player_t *p2 = w_add_player_from_info(
            &info);

        for (int32_t z = -32; z < 32; ++z) {
            for (int32_t x = -32; x < 32; ++x) {
                ivector3_t voxel_coord = ivector3_t((float)x, -2.0f, (float)z);
                ivector3_t chunk_coord = w_convert_voxel_to_chunk(voxel_coord);
                chunk_t *chunk = get_chunk(chunk_coord);
                chunk->flags.has_to_update_vertices = 1;
                ivector3_t local_coord = w_convert_voxel_to_local_chunk(voxel_coord);
                uint32_t index = get_voxel_index(local_coord.x, local_coord.y, local_coord.z);
                chunk->voxels[index] = 80;
            }
        }
    } break;

    case ATS_ROLLING: {


        
    } break;

    }

    ai.iteration_count += ai.ai_iteration_stride;

    ai.first_iteration = 0;

    return evolved;
#endif

    return 0;
}

static float s_calculate_score(
    player_t *p) {
    switch (ai.training_type) {

    case ATS_WALKING: {
        vector3_t destination = vector3_t(5.0f, 0.0f, 5.0f);

        vector3_t diff = destination - p->ws_position;

        float dot = glm::max(glm::dot(diff, diff), 0.00001f);

        return (1.0f / dot) * 10.0f;
    } break;

    default: {
    } break;

    }

    return 0.0f;
}

bool w_check_ai_population(
    float dt) {
#if 0
    if (ai.in_training) {
        stack_container_t<player_t *> &players = w_get_players();

        uint32_t total_ai_count = 0;
        uint32_t dead_ai = 0;

        for (uint32_t i = 0; i < players.data_count; ++i) {
            player_t *p = players[i];

            if (p) {
                if (p->ai) {
                    p->ai_start_time += dt;

                    if (p->ai) {
                        ++total_ai_count;
                    }

                    // Must have fallen off or something
                    if (p->flags.alive_state == PAS_DEAD) {
                        if (!p->calculated_score) {
                            float score = 0.0f;

                            assign_ai_score(p->ai_id, 0.0f);
                            p->calculated_score = 1;

                            // LOG_INFOV("Entity with AI %d died with score %f\n", p->ai_id, score);
                        }

                        ++dead_ai;
                    }
                    // Ran out of time
                    else if (p->ai_start_time > 3.0f) {
                        float score = s_calculate_score(p);

                        // Flag the AI as dead
                        assign_ai_score(p->ai_id, score);
                        p->flags.alive_state = PAS_DEAD;
                        p->calculated_score = 1;

                        // LOG_INFOV("Entity with AI %d died with score %f\n", ai.iteration_count, score);

                        ++dead_ai;
                    }
                }
            }
        }

        if (total_ai_count == dead_ai) {
            return s_reset_arena(ai.training_type);
        }
    }

    return 0;
#endif

    return 0;
}

void w_finish_generation() {
    for (;;) {
        float dt = 1.0f / 60.0f;
        dt *= 5.0f;

        if (w_check_ai_population(dt)) {
            break;
        }
    }
}

void w_begin_ai_training(
    ai_training_session_t session_type) {
    ai.in_training = 1;
    ai.training_type = session_type;

    ai.population_count = 150;

    begin_ai_training_population(ai.population_count);

    ai.iteration_count = 0;

    switch (session_type) {
    case ATS_WALKING: {
        ai.ai_iteration_stride = 1;
    } break;

    case ATS_ROLLING: {
        ai.ai_iteration_stride = 1;
    } break;
    }

    ai.first_iteration = 1;

    s_reset_arena(
        session_type);
}
