#pragma once

#include <common/tools.hpp>

// When an AI is attached to a player, much like how the network module
// pushes actions to the players, the AI module will also push actions
// to the players so that when it comes to updating the players, they
// will be filled with actions to execute

void ai_init();

void begin_ai_training_population(
    uint32_t population_size,
    uint32_t input_count,
    uint32_t output_count);

// When the AI dies, assign the score
void assign_ai_score(
    uint32_t *ai,
    float score);

// Need to choose the AI ID as well
void attach_ai(
    uint32_t player_id,
    uint32_t ai_id);

void evolve_population();
    
void end_ai_population();

// Pushes actions to the entities that have had ai attached to them
void tick_ai();
