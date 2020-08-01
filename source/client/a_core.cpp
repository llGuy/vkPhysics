#include "ai.hpp"
#include "common/player.hpp"
#include "world.hpp"
#include "a_internal.hpp"
#include <common/time.hpp>
#include "common/allocators.hpp"

struct ai_t {
    uint32_t player_id;
    uint32_t ai_id;

    time_stamp_t spawn_time;
};

static uint32_t current_in_training;
static neat_universe_t universe;
static uint32_t attached_ai_count;
static ai_t *attached_ais;
static bool in_training;

void ai_init() {
    // Initialises any sort of memory allocations, etc...
    a_neat_module_init();

    in_training = 0;
}

void begin_ai_training_population(
    uint32_t population_size) {
    a_universe_init(
        &universe,
        population_size,
        29,
        9);

    current_in_training = 0;

    attached_ais = FL_MALLOC(ai_t, population_size);

    in_training = 1;
}

bool is_in_training() {
    return in_training;
}

void assign_ai_score(
    uint32_t ai_index,
    float score) {
    ai_t *ai = &attached_ais[ai_index];
    
    neat_entity_t *n_ent = &universe.entities[ai->ai_id];
    n_ent->score = score;
}

uint32_t attach_ai(
    uint32_t player_id,
    uint32_t ai_id) {
    ai_t *ai = &attached_ais[ai_id];
    ai->player_id = player_id;
    ai->ai_id = ai_id;
    ai->spawn_time = current_time();

    return ai_id;
}

uint32_t attach_ai(
    uint32_t player_id) {
    uint32_t ai_index = attached_ai_count++;
    ai_t *ai = &attached_ais[ai_index];
    ai->player_id = player_id;
    ai->ai_id = ai_index;
    ai->spawn_time = current_time();

    return ai_index;
}

void iterate_ai(
    uint32_t ai_id,
    uint32_t iteration_count) {
    ai_t *ai = &attached_ais[ai_id];
    ai->ai_id = iteration_count;
}

uint32_t train_next_ai(
    uint32_t batch_size,
    uint32_t ai_id) {
    ai_t *ai = &attached_ais[ai_id];

    ai->ai_id += batch_size;

    if (ai->ai_id >= attached_ai_count) {
        return INVALID_AI_ID;
    }
    else {
        return ai->ai_id;
    }
}

void evolve_population() {
    a_end_evaluation_and_evolve(&universe);
}

void end_ai_population() {
    
}

void tick_ai() {
    float *inputs = LN_MALLOC(float, universe.neat.input_count);
    float *outputs = LN_MALLOC(float, universe.neat.output_count);

    memset(inputs, 0, sizeof(float) * universe.neat.input_count);
    memset(outputs, 0, sizeof(float) * universe.neat.output_count);

    // Loop through all attached AIs
    for (uint32_t i = 0; i < attached_ai_count; ++i) {
        ai_t *ai = &attached_ais[i];

        if (time_difference(current_time(), ai->spawn_time) > 10.0f) {
            uint32_t next_ai = train_next_ai(1, ai->ai_id);
        }

        neat_entity_t *n_ent = &universe.entities[ai->ai_id];
        player_t *p = get_player(ai->player_id);

        inputs[0] = p->ws_position.x;
        inputs[1] = p->ws_position.y;
        inputs[2] = p->ws_position.z;

        sensors_t sensors = {};
        sensors.s = &inputs[3];
        cast_ray_sensors(&sensors, p->ws_position, p->ws_view_direction, p->ws_up_vector);
        // Cast the rays to get the 26 sensors

        a_run_genome(&universe.neat, &n_ent->genome, inputs, outputs);

        player_action_t actions = {};
        actions.dt = logic_delta_time();

        actions.dmouse_x = outputs[0];
        actions.dmouse_y = outputs[1];

        if (outputs[2] > 0.5f) {
            actions.move_forward = 1;
        }

        if (outputs[3] > 0.5f) {
            actions.move_left = 1;
        }

        if (outputs[4] > 0.5f) {
            actions.move_back = 1;
        }

        if (outputs[5] > 0.5f) {
            actions.move_right = 1;
        }

        if (outputs[6] > 0.5f) {
            actions.jump = 1;
        }

        if (outputs[7] > 0.5f) {
            actions.crouch = 1;
        }

        if (outputs[8] > 0.5f) {
            actions.switch_shapes = 1;
        }

        push_player_actions(p, &actions, 0);
    }
}
