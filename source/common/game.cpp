#include "game.hpp"
#include "event.hpp"



static float dt;



void game_memory_init() {
    game->chunk_indices.init();

    game->max_modified_chunks = MAX_LOADED_CHUNKS / 2;
    game->modified_chunks = FL_MALLOC(chunk_t *, game->max_modified_chunks);
}

void game_begin(game_initiate_info_t *info) {
    game->player_scale = vector3_t(0.5f);

    // And do some various things depending on the game_info_t
}

void game_end() {
    
}

void join_game() {
    
}

void leave_game() {
    
}

void timestep_begin(float dt) {
    game->dt = dt;
}

void timestep_end() {
    game->dt = 0.0f;
}

float get_timestep_delta() {
    return game->dt;
}

chunk_t *get_chunk(const ivector3_t &coord) {
    
}

chunk_t *access_chunk(const ivector3_t &coord) {
    
}

player_t *add_player() {
    uint32_t player_index = game->players.add();
    game->players[player_index] = FL_MALLOC(player_t, 1);
    player_t *player_ptr = game->players[player_index];
    memset(player_ptr, 0, sizeof(player_t));
    player_ptr->local_id = player_index;

    return player_ptr;
}

#define TERRAFORMING_SPEED 200.0f

void push_player_actions(player_t *player, player_action_t *action, bool override_adt) {
    if (player->player_action_count < MAX_PLAYER_ACTIONS) {
        if (override_adt) {
            player->player_actions[player->player_action_count++] = *action;
        }
        else {
            player->accumulated_dt += action->dt;

            float max_change = 1.0f * player->accumulated_dt * TERRAFORMING_SPEED;
            int32_t max_change_i = (int32_t)max_change;

            if (max_change_i < 2) {
                action->accumulated_dt = 0.0f;
            }
            else {
                action->accumulated_dt = player->accumulated_dt;
                player->accumulated_dt = 0.0f;
            }

            player->player_actions[player->player_action_count++] = *action;
        }
    }
    else {
        // ...
        LOG_WARNING("Too many player actions\n");
    }
}

static void s_handle_shape_switch(
    player_t *player,
    player_action_t *actions) {
    if (actions->switch_shapes) {
        // If already switching shapes
        if (player->switching_shapes)
            player->shape_switching_time = SHAPE_SWITCH_ANIMATION_TIME - player->shape_switching_time;
        else
            player->shape_switching_time = 0.0f;

        if (player->flags.interaction_mode == PIM_STANDING) {
            player->flags.interaction_mode = PIM_BALL;
            player->switching_shapes = 1;
        }
        else if (player->flags.interaction_mode == PIM_BALL) {
            player->flags.interaction_mode = PIM_STANDING;
            player->switching_shapes = 1;
        }
    }

    if (player->switching_shapes)
        player->shape_switching_time += game->dt;

    if (player->shape_switching_time > PLAYER_SHAPE_SWITCH_DURATION) {
        player->shape_switching_time = 0.0f;
        player->switching_shapes = 0;
    }
}

// Special abilities like 
static void s_execute_player_triggers(
    player_t *player,
    player_actions_t *player_actions) {
    player->terraform_package = w_cast_terrain_ray(
        player->ws_position,
        player->ws_view_direction,
        10.0f);

    if (player_actions->trigger_left) {
#if NET_DEBUG
        LOG_INFOV("(Tick %llu) Terraforming (adt %f) p: %s d:%s\n",
                  (unsigned long long)player_actions->tick,
                  player_actions->accumulated_dt,
                  glm::to_string(player->ws_position).c_str(),
                  glm::to_string(player->ws_view_direction).c_str());
#endif

        w_terraform(
            TT_DESTROY,
            player->terraform_package,
            TERRAFORMING_RADIUS,
            TERRAFORMING_SPEED,
            player_actions->accumulated_dt);
    }
    if (player_actions->trigger_right) {
        w_terraform(
            TT_BUILD,
            player->terraform_package,
            TERRAFORMING_RADIUS,
            TERRAFORMING_SPEED,
            player_actions->accumulated_dt);
    }

    if (player_actions->flashlight) {
        player->flags.flashing_light ^= 1;
    }
}

bool execute_action(player_t *player, player_action_t *action, event_submissions_t *events) {
    // Shape switching can happen regardless of the interaction mode
    s_handle_shape_switch(player, action);

    switch (player->flags.interaction_mode) {
            
    case PIM_METEORITE: {
        s_execute_player_direction_change(player, action);
        s_accelerate_meteorite_player(player, action);
    } break;

        // FOR NOW STANDING AND BALL ARE EQUIVALENT ///////////////////////
    case PIM_STANDING: {
        s_execute_player_triggers(player, action);
        s_execute_player_direction_change(player, action);
        s_execute_standing_player_movement(player, action);

        s_check_player_dead(action, player, events);
    } break;

    case PIM_BALL: {
        s_execute_player_direction_change(player, action);
        s_execute_ball_player_movement(player, action);

        s_check_player_dead(action, player, events);
    } break;

    case PIM_FLOATING: {
        s_execute_player_triggers(player, action);
        s_execute_player_direction_change(player, action);
        s_execute_player_floating_movement(player, action);
    } break;

    default: {
    } break;

    }
}
