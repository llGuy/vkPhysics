#pragma once

#include <common/tools.hpp>

void ai_init();

struct ai_t {
    uint32_t neat_entity_id;
};

void begin_ai_population(
    uint32_t population_size,
    uint32_t input_count,
    uint32_t output_count);

void evolve_population();
    
void end_ai_population();
