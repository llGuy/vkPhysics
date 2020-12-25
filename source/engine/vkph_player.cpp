#include "vkph_state.hpp"
#include "vkph_player.hpp"

namespace vkph {

void player_t::init_default_values(player_init_info_t *info) {
    ws_position = info->ws_position;
    ws_view_direction = info->ws_view_direction;
    ws_up_vector = info->ws_up_vector;
    player_action_count = 0;
    default_speed = info->default_speed;
    next_random_spawn_position = info->next_random_spawn_position;
    ball_speed = 0.0f;
    ws_velocity = vector3_t(0.0f);
    idx_in_chunk_list = -1;
    health = 200;
    snapshot_before = snapshot_before = 0;

    // FOR NOW: just have the default weapon setup

    selected_weapon = 0;
    init_weapons();
}

void player_t::init_weapons() {
    // TODO: Configure weapons based on selected class.
    weapon_count = 2;
    weapons[0].init(40, 40, firing_type_t::SEMI_AUTOMATIC, bullet_type_t::PROJECTILE, weapon_type_t::ROCKS, 5.0f);
    weapons[1].init(0, 0, firing_type_t::INVALID, bullet_type_t::INVALID, weapon_type_t::TERRAFORMER, 0);
}

void player_t::init(player_init_info_t *info, int16_t *client_to_local_id_map) {
    if (info->client_name) {
        name = info->client_name;
        client_id = info->client_id;
        client_to_local_id_map[info->client_id] = local_id;
    }

    init_default_values(info);

    memset(player_actions, 0, sizeof(player_actions));
    accumulated_dt = 0.0f;

    flags.u32 = info->flags;
}

void player_t::push_actions(player_action_t *action, bool override_adt) {
    if (player_action_count < PLAYER_MAX_ACTIONS_COUNT) {
        if (override_adt) {
            player_actions[player_action_count++] = *action;
        }
        else {
            accumulated_dt += action->dt;

            float max_change = 1.0f * accumulated_dt * PLAYER_TERRAFORMING_SPEED;
            int32_t max_change_i = (int32_t)max_change;

            if (max_change_i < 2) {
                action->accumulated_dt = 0.0f;
            }
            else {
                action->accumulated_dt = accumulated_dt;
                accumulated_dt = 0.0f;
            }

            player_actions[player_action_count++] = *action;
        }
    }
    else {
        // ...
        LOG_WARNING("Too many player actions\n");
    }
}

void player_t::execute_action(player_action_t *action, state_t *state) {
    // Shape switching can happen regardless of the interaction mode
    handle_shape_switch(action->switch_shapes, action->dt);

    switch (flags.interaction_mode) {
            
    case PIM_METEORITE: {
        execute_player_direction_change(action);
        accelerate_meteorite_player(action);
    } break;

        // FOR NOW STANDING AND BALL ARE EQUIVALENT ///////////////////////
    case PIM_STANDING: {
        execute_player_triggers(action);
        execute_player_direction_change(action);
        execute_standing_player_movement(action);

        check_player_dead(action);
    } break;

    case PIM_BALL: {
        execute_player_direction_change(action);
        execute_ball_player_movement(action);

        check_player_dead(action);
    } break;

    case PIM_FLOATING: {
        execute_player_triggers(action);
        execute_player_direction_change(action);
        execute_player_floating_movement(action);
    } break;

    default: {
    } break;

    }

    for (uint32_t i = 0; i < weapon_count; ++i) {
        weapons[i].elapsed += action->dt;
    }

    update_player_chunk_status();
}

vector3_t player_t::compute_view_position() {
    return ws_position + ws_up_vector * PLAYER_SCALE * 2.0f;
}

void player_t::handle_shape_switch(bool switch_shapes, float dt) {
    if (switch_shapes) {
        // If already switching shapes
        if (switching_shapes)
            shape_switching_time = SHAPE_SWITCH_ANIMATION_TIME - shape_switching_time;
        else
            shape_switching_time = 0.0f;

        if (flags.interaction_mode == PIM_STANDING) {
            flags.interaction_mode = PIM_BALL;
            switching_shapes = 1;
        }
        else if (flags.interaction_mode == PIM_BALL) {
            flags.interaction_mode = PIM_STANDING;
            switching_shapes = 1;

            // Need to set animation to the "STOP" animation if the velocity exceeds a certain value
            if (frame_displacement / dt > 4.0f)
                animated_state = PAS_STOP_FAST;
        }
    }

    if (switching_shapes)
        shape_switching_time += dt;

    if (shape_switching_time > PLAYER_SHAPE_SWITCH_DURATION) {
        shape_switching_time = 0.0f;
        switching_shapes = 0;
    }
}

void player_t::handle_weapon_switch(player_action_t *actions) {
    if (actions->switch_weapons) {
        if (actions->next_weapon == 0b111) {
            // Just cycle through to next weapon
            selected_weapon = (selected_weapon + 1) % weapon_count;
            LOG_INFOV("Player chose weapon: %d\n", selected_weapon);
        }
        else if (actions->next_weapon < weapon_count) {
            selected_weapon = actions->next_weapon;
            LOG_INFOV("Player chose weapon: %d\n", selected_weapon);
        }
    }
}

void player_t::execute_player_triggers(player_action_t *player_actions, state_t *state) {
    handle_weapon_switch(player_actions);

    weapon_t *weapon = &weapons[selected_weapon];

    switch (weapon->type) {
    case weapon_type_t::ROCKS: {
        // Spawn rock
        if (player_actions->trigger_left && weapon->elapsed > weapon->recoil_time) {
            weapon->elapsed = 0.0f;
            
            // TODO: Do check to see if the player can shoot...
            uint32_t ref_idx = weapon->active_projs.add();

            /*
              Spawn rock in the GAME STATE.
             */
            uint32_t rock_idx = state->rocks.spawn(
                compute_view_position(),
                ws_view_direction * PROJECTILE_ROCK_SPEED,
                ws_up_vector,
                client_id,
                ref_idx,
                selected_weapon);

            weapon->active_projs[ref_idx].initialised = 1;
            weapon->active_projs[ref_idx].idx = rock_idx;
        }

        terraform_package.ray_hit_terrain = 0;
    } break;

        // Special case
    case weapon_type_t::TERRAFORMER: {
        terraform_package = cast_terrain_ray(
            compute_view_position(),
            ws_view_direction,
            10.0f,
            terraform_package.color);

        if (player_actions->trigger_left)
            terraform(TT_DESTROY, terraform_package, PLAYER_TERRAFORMING_RADIUS, PLAYER_TERRAFORMING_SPEED, player_actions->accumulated_dt);
        if (player_actions->trigger_right)
            terraform(TT_BUILD, terraform_package, PLAYER_TERRAFORMING_RADIUS, PLAYER_TERRAFORMING_SPEED, player_actions->accumulated_dt);

        if (player_actions->flashlight) {
            flags.flashing_light ^= 1;
        }
    } break;
    }
}

}
