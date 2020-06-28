#pragma once

#include <common/tools.hpp>

// The inputs to the neural network are:
// - 3 floats for the position of the player
// - 3 floats for the view direction of the player
// - 3 floats for the up vector of the player
// - 26 floats for each ray that is cast from the player's position
// - need to figure out how we are going to handle the different players in the world

// The outputs of the neural network are:
// - Horizontal mouse movement
// - Vertical mouse movement
// - Forward button
// - Left button
// - Back button
// - Right button
// - Jump button
// - Crouch button
// - Switch shape button

// When an AI is attached to a player, much like how the network module
// pushes actions to the players, the AI module will also push actions
// to the players so that when it comes to updating the players, they
// will be filled with actions to execute
void ai_init();

void begin_ai_training_population(
    uint32_t population_size);

// When the AI dies, assign the score
void assign_ai_score(
    uint32_t ai,
    float score);

// Need to choose the AI ID as well
uint32_t attach_ai(
    uint32_t player_id,
    uint32_t ai_id);

#define INVALID_AI_ID 0xFFFFFFFF

// If 0xFFFF is returned, we need to evolve
// Need to make sure that batch_size is a factor of the population_size
uint32_t train_next_ai(
    uint32_t batch_size,
    uint32_t ai_id);

void evolve_population();
    
void end_ai_population();

// Pushes actions to the entities that have had ai attached to them
void tick_ai();
