#include "dr_rsc.hpp"
#include "cl_main.hpp"
#include "dr_chunk.hpp"
#include "dr_player.hpp"
#include "wd_predict.hpp"
#include "dr_draw_scene.hpp"
#include <common/player.hpp>
#include <renderer/renderer.hpp>

static void s_render_person(
    VkCommandBuffer render_command_buffer,
    VkCommandBuffer transfer_command_buffer,
    player_t *p) {
    if (p->render->animations.next_bound_cycle != p->animated_state) {
        switch_to_cycle(
            &p->render->animations,
            p->animated_state,
            0);
    }

    interpolate_joints(&p->render->animations, cl_delta_time(), dr_is_animation_repeating((player_animated_state_t)p->animated_state));
    sync_gpu_with_animated_transforms(&p->render->animations, transfer_command_buffer);

    // This has to be a bit different
    movement_axes_t axes = compute_movement_axes(p->ws_view_direction, p->ws_up_vector);
    matrix3_t normal_rotation_matrix3 = (matrix3_t(glm::normalize(axes.right), glm::normalize(axes.up), glm::normalize(-axes.forward)));
    matrix4_t normal_rotation_matrix = matrix4_t(normal_rotation_matrix3);
    normal_rotation_matrix[3][3] = 1.0f;

    vector3_t view_dir = glm::normalize(p->ws_view_direction);
    float dir_x = view_dir.x;
    float dir_z = view_dir.z;
    float rotation_angle = atan2(dir_z, dir_x);

    matrix4_t rot_matrix = glm::rotate(-rotation_angle, vector3_t(0.0f, 1.0f, 0.0f));

    p->render->render_data.model = glm::translate(p->ws_position) *
        normal_rotation_matrix *
        p->render->animations.cycles->rotation *
        glm::scale(PLAYER_SCALE) *
        p->render->animations.cycles->scale;

    submit_skeletal_mesh(
        render_command_buffer,
        dr_get_mesh_rsc(GM_PLAYER),
        dr_get_shader_rsc(GS_PLAYER),
        &p->render->render_data,
        &p->render->animations);
}

static void s_render_ball(
    VkCommandBuffer render_command_buffer,
    VkCommandBuffer shadow_render_command_buffer,
    player_t *p) {
    p->render->rotation_speed = p->frame_displacement / calculate_sphere_circumference(PLAYER_SCALE.x) * 360.0f;
    p->render->rotation_angle += p->render->rotation_speed;

    if (p->render->rotation_angle > 360.0f) {
        p->render->rotation_angle -= 360.0f;
    }

    if (glm::dot(p->ws_velocity, p->ws_velocity) > 0.0001f) {
        vector3_t cross = glm::cross(glm::normalize(p->ws_velocity), p->ws_up_vector);
        vector3_t right = glm::normalize(cross);

        matrix4_t rolling_rotation = glm::rotate(glm::radians(p->render->rotation_angle), -right);

        p->render->rolling_matrix = rolling_rotation;
    }

    p->render->render_data.model = glm::translate(p->ws_position) * p->render->rolling_matrix * glm::scale(PLAYER_SCALE);

    begin_mesh_submission(render_command_buffer, dr_get_shader_rsc(GS_BALL));
    submit_mesh(
        render_command_buffer,
        dr_get_mesh_rsc(GM_BALL),
        dr_get_shader_rsc(GS_BALL),
        &p->render->render_data);

    submit_mesh_shadow(
        shadow_render_command_buffer,
        dr_get_mesh_rsc(GM_BALL),
        dr_get_shader_rsc(GS_BALL_SHADOW),
        &p->render->render_data);
}

static void s_render_transition(
    VkCommandBuffer render_command_buffer,
    VkCommandBuffer transfer_command_buffer,
    player_t *p) {
    struct {
        matrix4_t first_model;
        matrix4_t second_model;

        vector4_t color;
        vector4_t pbr_info;
    
        // To add later with texture stuff
        int32_t texture_index;

        float progression;
        
    } render_data;

    if (p->render->animations.next_bound_cycle != p->animated_state) {
        switch_to_cycle(
            &p->render->animations,
            p->animated_state,
            0);
    }

    interpolate_joints(&p->render->animations, cl_delta_time(), dr_is_animation_repeating((player_animated_state_t)p->animated_state));
    sync_gpu_with_animated_transforms(&p->render->animations, transfer_command_buffer);

    // This has to be a bit different
    movement_axes_t axes = compute_movement_axes(p->ws_view_direction, p->ws_up_vector);
    matrix3_t normal_rotation_matrix3 = (matrix3_t(glm::normalize(axes.right), glm::normalize(axes.up), glm::normalize(-axes.forward)));
    matrix4_t normal_rotation_matrix = matrix4_t(normal_rotation_matrix3);
    normal_rotation_matrix[3][3] = 1.0f;

    vector3_t view_dir = glm::normalize(p->ws_view_direction);
    float dir_x = view_dir.x;
    float dir_z = view_dir.z;
    float rotation_angle = atan2(dir_z, dir_x);

    matrix4_t rot_matrix = glm::rotate(-rotation_angle, vector3_t(0.0f, 1.0f, 0.0f));
    // render_data.first_model = glm::translate(p->ws_position) * normal_rotation_matrix * glm::scale(w_get_player_scale());
    render_data.first_model = glm::translate(p->ws_position) *
        normal_rotation_matrix *
        p->render->animations.cycles->rotation *
        glm::scale(PLAYER_SCALE) *
        p->render->animations.cycles->scale;

    p->render->rotation_speed = p->frame_displacement / calculate_sphere_circumference(PLAYER_SCALE.x) * 360.0f;
    p->render->rotation_angle += p->render->rotation_speed;

    if (p->render->rotation_angle > 360.0f) {
        p->render->rotation_angle -= 360.0f;
    }

    if (glm::dot(p->ws_velocity, p->ws_velocity) > 0.0001f) {
        vector3_t cross = glm::cross(glm::normalize(p->ws_velocity), p->ws_up_vector);
        vector3_t right = glm::normalize(cross);

        matrix4_t rolling_rotation = glm::rotate(glm::radians(p->render->rotation_angle), -right);

        p->render->rolling_matrix = rolling_rotation;
    }

    render_data.second_model = glm::translate(p->ws_position) * p->render->rolling_matrix * glm::scale(PLAYER_SCALE);

    render_data.color = vector4_t(1.0f);
    render_data.pbr_info.x = 0.1f;
    render_data.pbr_info.y = 0.1f;
    render_data.progression = p->shape_switching_time / SHAPE_SWITCH_ANIMATION_TIME;

    if (p->flags.interaction_mode == PIM_STANDING) {
        // Need to render transition from ball to person
        submit_skeletal_mesh(
            render_command_buffer,
            dr_get_mesh_rsc(GM_MERGED),
            dr_get_shader_rsc(GS_MERGED_BALL),
            &render_data,
            sizeof(render_data),
            &p->render->animations);
    }
    else {
        // Need to render transition from person to ball
        submit_skeletal_mesh(
            render_command_buffer,
            dr_get_mesh_rsc(GM_MERGED),
            dr_get_shader_rsc(GS_MERGED_PLAYER),
            &render_data,
            sizeof(render_data),
            &p->render->animations);
    }
}

// When skeletal animation is implemented, this function will do stuff like handle that
static void s_players_gpu_sync_and_render(
    VkCommandBuffer render_command_buffer,
    VkCommandBuffer render_shadow_command_buffer,
    VkCommandBuffer transfer_command_buffer) {
    for (uint32_t i = 0; i < get_player_count(); ++i) {
        player_t *p = get_player(i);
        if (p) {
            if (p->flags.alive_state == PAS_ALIVE) {
                if (!p->render) {
                    p->render = dr_player_render_init();
                }

                p->render->render_data.color = vector4_t(1.0f);
                p->render->render_data.pbr_info.x = 0.1f;
                p->render->render_data.pbr_info.y = 0.1f;

                if ((int32_t)i == (int32_t)wd_get_local_player()) {
                    if (p->switching_shapes) {
                        // Render transition
                        s_render_transition(render_command_buffer, transfer_command_buffer, p);
                    }
                    else if (p->flags.interaction_mode == PIM_STANDING) {
                        s_render_person(render_command_buffer, transfer_command_buffer, p);
                    }
                    else {
                        s_render_ball(render_command_buffer, render_shadow_command_buffer, p);
                    } 
                    // TODO: Special handling for first person mode
                }
                else {
                    if (p->switching_shapes) {
                        // Render transition
                        s_render_transition(render_command_buffer, transfer_command_buffer, p);
                    }
                    else if (p->flags.interaction_mode == PIM_STANDING) {
                        s_render_person(render_command_buffer, transfer_command_buffer, p);
                    }
                    else {
                        s_render_ball(render_command_buffer, render_shadow_command_buffer, p);
                    } 
                }
            }
        }
    }
}

static void s_chunks_gpu_sync_and_render(
    VkCommandBuffer render_command_buffer,
    VkCommandBuffer transfer_command_buffer) {
    const uint32_t max_chunks_loaded_per_frame = 10;
    uint32_t chunks_loaded = 0;

    terraform_package_t *current_terraform_package = wd_get_local_terraform_package();

    if (current_terraform_package->ray_hit_terrain) {
        dr_chunk_colors_g.chunk_color.pointer_radius = 3.0f;
    }
    else {
        dr_chunk_colors_g.chunk_color.pointer_radius = 0.0f;
    }

    dr_chunk_colors_g.chunk_color.pointer_position = vector4_t(current_terraform_package->ws_position, 1.0f);
    dr_chunk_colors_g.chunk_color.pointer_color = vector4_t(0.0f, 1.0f, 1.0f, 1.0f);

    update_gpu_buffer(
        transfer_command_buffer,
        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
        0,
        sizeof(chunk_color_data_t),
        &dr_chunk_colors_g.chunk_color,
        &dr_chunk_colors_g.chunk_color_ubo);

    begin_mesh_submission(
        render_command_buffer,
        dr_get_shader_rsc(GS_CHUNK),
        dr_chunk_colors_g.chunk_color_set);

    uint32_t chunk_count;
    chunk_t **chunks = get_active_chunks(&chunk_count);
    uint8_t surface_level = CHUNK_SURFACE_LEVEL;

    for (uint32_t i = 0; i < chunk_count; ++i) {
        chunk_t *c = chunks[i];
        if (c) {
            if (c->flags.has_to_update_vertices && chunks_loaded < max_chunks_loaded_per_frame) {
                c->flags.has_to_update_vertices = 0;
                // Update chunk mesh and put on GPU + send to command buffer
                // TODO:
                dr_update_chunk_draw_rsc(
                    transfer_command_buffer,
                    surface_level,
                    c);

                ++chunks_loaded;
            }
        
            if (c->flags.active_vertices) {
                submit_mesh(
                    render_command_buffer,
                    &c->render->mesh,
                    dr_get_shader_rsc(GS_CHUNK),
                    &c->render->render_data);
            }
        }
    }
}

void dr_draw_game(
    VkCommandBuffer render,
    VkCommandBuffer transfer,
    VkCommandBuffer shadow) {
    s_players_gpu_sync_and_render(render, shadow, transfer);
    s_chunks_gpu_sync_and_render(render, transfer);

    if (render != VK_NULL_HANDLE) {
        render_environment(render);
    }
}

void dr_draw_premade_scene(
    VkCommandBuffer render,
    VkCommandBuffer transfer,
    fixed_premade_scene_t *scene) {
    begin_mesh_submission(
        render,
        dr_get_shader_rsc(GS_CHUNK),
        dr_chunk_colors_g.chunk_color_set);

    submit_mesh(
        render,
        &scene->world_mesh,
        dr_get_shader_rsc(GS_CHUNK),
        &scene->world_render_data);
}
