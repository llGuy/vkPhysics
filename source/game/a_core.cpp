#include "ai.hpp"
#include "world.hpp"
#include "a_internal.hpp"
#include "common/allocators.hpp"

struct ai_t {
    uint32_t player_id;
    uint32_t ai_id;
};

static uint32_t current_in_training;
static neat_universe_t universe;
static uint32_t attached_ai_count;
static ai_t *attached_ais;

void ai_init() {
    // Initialises any sort of memory allocations, etc...
    a_neat_module_init();
}

void begin_ai_population(
    uint32_t population_size,
    uint32_t input_count,
    uint32_t output_count) {
    a_universe_init(
        &universe,
        population_size,
        input_count,
        output_count);

    current_in_training = 0;

    attached_ais = FL_MALLOC(ai_t, population_size);
}

void assign_ai_score(
    ai_t *ai,
    float score) {
    
}

void attach_ai(
    uint32_t player) {
    
}

void evolve_population() {
    
}

void end_ai_population() {
    
}

void tick_ai() {
    float *inputs = LN_MALLOC(float, universe.neat.input_count);
    float *outputs = LN_MALLOC(float, universe.neat.output_count);

    // Loop through all attached AIs
    for (uint32_t i = 0; i < attached_ai_count; ++i) {
        ai_t *ai = &attached_ais[i];

        neat_entity_t *n_ent = &universe.entities[ai->ai_id];

        a_run_genome(&universe.neat, &n_ent->genome, inputs, outputs);
    }
}
