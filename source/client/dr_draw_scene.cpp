#include "sc_play.hpp"
#include <vkph_state.hpp>
#include <vkph_team.hpp>
#include <vkph_physics.hpp>
#include "dr_rsc.hpp"
#include "cl_main.hpp"
#include "sc_scene.hpp"
#include "dr_chunk.hpp"
#include "dr_player.hpp"
#include "wd_predict.hpp"
#include "dr_draw_scene.hpp"
#include <app.hpp>
#include <vkph_player.hpp>
#include <vk.hpp>

static void s_render_person(
    VkCommandBuffer render,
    VkCommandBuffer shadow,
    VkCommandBuffer transfer,
    vkph::player_t *p) {
    if (p->render->animations.next_bound_cycle != p->animated_state)
        p->render->animations.switch_to_cycle(p->animated_state, 0);

    p->render->animations.interpolate_joints(
        cl_delta_time(),
        dr_is_animation_repeating((vkph::player_animated_state_t)p->animated_state));

    p->render->animations.sync_gpu_with_transforms(transfer);

    // This has to be a bit different
    vkph::movement_axes_t axes = vkph::compute_movement_axes(p->ws_view_direction, p->ws_up_vector);
    matrix3_t normal_rotation_matrix3 = (matrix3_t(glm::normalize(axes.right), glm::normalize(axes.up), glm::normalize(-axes.forward)));
    matrix4_t normal_rotation_matrix = matrix4_t(normal_rotation_matrix3);
    normal_rotation_matrix[3][3] = 1.0f;

    matrix4_t tran = glm::translate(p->ws_position);
    matrix4_t rot = normal_rotation_matrix * p->render->animations.cycles->rotation;
    matrix4_t sca = glm::scale(vector3_t(vkph::PLAYER_SCALE)) * p->render->animations.cycles->scale;

    p->render->render_data.model = tran * rot * sca;
    buffer_t rdata = {&p->render->render_data, vk::DEF_MESH_RENDER_DATA_SIZE};

    vk::submit_skeletal_mesh(render, dr_get_mesh_rsc(GM_PLAYER), dr_get_shader_rsc(GS_PLAYER), rdata, &p->render->animations);
    vk::submit_skeletal_mesh_shadow(shadow, dr_get_mesh_rsc(GM_PLAYER), dr_get_shader_rsc(GS_PLAYER_SHADOW), rdata, &p->render->animations);
}

static void s_render_ball(
    VkCommandBuffer render,
    VkCommandBuffer shadow,
    vkph::player_t *p) {
    p->render->rotation_speed = p->frame_displacement / calculate_sphere_circumference(vkph::PLAYER_SCALE) * 360.0f;
    p->render->rotation_angle += p->render->rotation_speed;

    if (p->render->rotation_angle > 360.0f)
        p->render->rotation_angle -= 360.0f;

    if (glm::dot(p->ws_velocity, p->ws_velocity) > 0.0001f) {
        vector3_t cross = glm::cross(glm::normalize(p->ws_velocity), p->ws_up_vector);
        vector3_t right = glm::normalize(cross);
        matrix4_t rolling_rotation = glm::rotate(glm::radians(p->render->rotation_angle), -right);
        p->render->rolling_matrix = rolling_rotation;
    }

    p->render->render_data.model = glm::translate(p->ws_position) * p->render->rolling_matrix * glm::scale(vector3_t(vkph::PLAYER_SCALE));
    buffer_t rdata = {&p->render->render_data, vk::DEF_MESH_RENDER_DATA_SIZE};

    vk::begin_mesh_submission(render, dr_get_shader_rsc(GS_BALL));
    vk::submit_mesh(render, dr_get_mesh_rsc(GM_BALL), dr_get_shader_rsc(GS_BALL), rdata);
    vk::submit_mesh_shadow(shadow, dr_get_mesh_rsc(GM_BALL), dr_get_shader_rsc(GS_BALL_SHADOW), {rdata});
}

static void s_render_transition(
    VkCommandBuffer render_command_buffer,
    VkCommandBuffer render_shadow_command_buffer,
    VkCommandBuffer transfer_command_buffer,
    vkph::player_t *p) {
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
        p->render->animations.switch_to_cycle(p->animated_state, 0);
    }

    p->render->animations.interpolate_joints(cl_delta_time(), dr_is_animation_repeating((vkph::player_animated_state_t)p->animated_state));
    p->render->animations.sync_gpu_with_transforms(transfer_command_buffer);

    // This has to be a bit different
    vkph::movement_axes_t axes = vkph::compute_movement_axes(p->ws_view_direction, p->ws_up_vector);
    matrix3_t normal_rotation_matrix3 = (matrix3_t(glm::normalize(axes.right), glm::normalize(axes.up), glm::normalize(-axes.forward)));
    matrix4_t normal_rotation_matrix = matrix4_t(normal_rotation_matrix3);
    normal_rotation_matrix[3][3] = 1.0f;

    vector3_t view_dir = glm::normalize(p->ws_view_direction);
    float dir_x = view_dir.x;
    float dir_z = view_dir.z;
    float rotation_angle = atan2(dir_z, dir_x);

    matrix4_t rot_matrix = glm::rotate(-rotation_angle, vector3_t(0.0f, 1.0f, 0.0f));
    // render_data.first_model = glm::translate(p->ws_position) * normal_rotation_matrix * glm::scale(w_state->get_player_scale());
    render_data.first_model = glm::translate(p->ws_position) *
        normal_rotation_matrix *
        p->render->animations.cycles->rotation *
        glm::scale(vector3_t(vkph::PLAYER_SCALE)) *
        p->render->animations.cycles->scale;

    p->render->rotation_speed = p->frame_displacement / calculate_sphere_circumference(vkph::PLAYER_SCALE) * 360.0f;
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

    render_data.second_model = glm::translate(p->ws_position) * p->render->rolling_matrix * glm::scale(vector3_t(vkph::PLAYER_SCALE));

    render_data.color = p->render->render_data.color;
    render_data.pbr_info.x = 0.1f;
    render_data.pbr_info.y = 0.1f;
    render_data.progression = p->shape_switching_time / vkph::SHAPE_SWITCH_ANIMATION_TIME;

    if (p->flags.interaction_mode == vkph::PIM_STANDING) {
        // Need to render transition from ball to person
        submit_skeletal_mesh(
            render_command_buffer,
            dr_get_mesh_rsc(GM_MERGED),
            dr_get_shader_rsc(GS_MERGED_BALL),
            {&render_data, sizeof(render_data)},
            &p->render->animations);

        submit_skeletal_mesh_shadow(
            render_shadow_command_buffer,
            dr_get_mesh_rsc(GM_MERGED),
            dr_get_shader_rsc(GS_MERGED_BALL_SHADOW),
            {&render_data, sizeof(render_data)},
            &p->render->animations);
    }
    else {
        // Need to render transition from person to ball
        submit_skeletal_mesh(
            render_command_buffer,
            dr_get_mesh_rsc(GM_MERGED),
            dr_get_shader_rsc(GS_MERGED_PLAYER),
            {&render_data, sizeof(render_data)},
            &p->render->animations);

        submit_skeletal_mesh_shadow(
            render_shadow_command_buffer,
            dr_get_mesh_rsc(GM_MERGED),
            dr_get_shader_rsc(GS_MERGED_PLAYER_SHADOW),
            {&render_data, sizeof(render_data)},
            &p->render->animations);
    }
}

// When skeletal animation is implemented, this function will do stuff like handle that
static void s_players_gpu_sync_and_render(
    VkCommandBuffer render_command_buffer,
    VkCommandBuffer render_shadow_command_buffer,
    VkCommandBuffer transfer_command_buffer,
    vkph::state_t *state) {
    for (uint32_t i = 0; i < state->players.data_count; ++i) {
        vkph::player_t *p = state->get_player(i);
        if (p) {
            if (p->flags.is_alive) {
                if (!p->render) {
                    p->render = dr_player_render_init();
                }

                p->render->render_data.color = team_color_to_v4((vkph::team_color_t)p->flags.team_color);
                p->render->render_data.pbr_info.x = 0.1f;
                p->render->render_data.pbr_info.y = 0.1f;

                if ((int32_t)i == (int32_t)wd_get_local_player()) {
                    if (!sc_is_in_first_person()) {
                        if (p->switching_shapes) {
                            // Render transition
                            s_render_transition(render_command_buffer, render_shadow_command_buffer, transfer_command_buffer, p);
                        }
                        else if (p->flags.interaction_mode == vkph::PIM_STANDING) {
                            s_render_person(render_command_buffer, render_shadow_command_buffer, transfer_command_buffer, p);
                        }
                        else {
                            s_render_ball(render_command_buffer, render_shadow_command_buffer, p);
                        } 
                    }
                }
                else {
                    if (p->switching_shapes) {
                        s_render_transition(render_command_buffer, render_shadow_command_buffer, transfer_command_buffer, p);
                    }
                    else if (p->flags.interaction_mode == vkph::PIM_STANDING) {
                        s_render_person(render_command_buffer, render_shadow_command_buffer, transfer_command_buffer, p);
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
    struct frustum_t *frustum,
    VkCommandBuffer render_command_buffer,
    VkCommandBuffer render_shadow_command_buffer,
    VkCommandBuffer transfer_command_buffer,
    vkph::state_t *state) {
    const uint32_t max_chunks_loaded_per_frame = 10;
    uint32_t chunks_loaded = 0;

    vkph::terraform_package_t *current_terraform_package = wd_get_local_terraform_package();

    if (current_terraform_package->ray_hit_terrain) {
        dr_chunk_colors_g.chunk_color.pointer_radius = 3.0f;
    }
    else {
        dr_chunk_colors_g.chunk_color.pointer_radius = 0.0f;
    }

    dr_chunk_colors_g.chunk_color.pointer_position = vector4_t(current_terraform_package->ws_contact_point, 1.0f);
    dr_chunk_colors_g.chunk_color.pointer_color = vector4_t(0.0f, 1.0f, 1.0f, 1.0f);

    dr_chunk_colors_g.chunk_color_ubo.update(
        transfer_command_buffer,
        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
        0,
        sizeof(chunk_color_data_t),
        &dr_chunk_colors_g.chunk_color);

    vk::begin_mesh_submission(
        render_command_buffer,
        dr_get_shader_rsc(GS_CHUNK),
        dr_chunk_colors_g.chunk_color_set);

    uint32_t chunk_count;
    vkph::chunk_t **chunks = state->get_active_chunks(&chunk_count);
    uint8_t surface_level = vkph::CHUNK_SURFACE_LEVEL;

    const vk::eye_3d_info_t *eye_info = sc_get_eye_info();

    for (uint32_t i = 0; i < chunk_count; ++i) {
        vkph::chunk_t *c = chunks[i];
        if (c) {
            if (c->flags.has_to_update_vertices && chunks_loaded < max_chunks_loaded_per_frame) {
                c->flags.has_to_update_vertices = 0;
                // Update chunk mesh and put on GPU + send to command buffer
                // TODO:
                dr_update_chunk_draw_rsc(
                    transfer_command_buffer,
                    surface_level,
                    c,
                    NULL,
                    state);

                ++chunks_loaded;
            }
        
            if (c->flags.active_vertices) {
                // Make sure that the chunk is in the view of the frustum
                vector3_t chunk_center = vector3_t((c->chunk_coord * 16)) + vector3_t(8.0f);
                vector3_t diff = chunk_center - eye_info->position;
                // if (frustum_check_cube(frustum, chunk_center, 8.0f) || glm::dot(diff, diff) < 32.0f * 32.0f) {
                vk::submit_mesh(
                    render_command_buffer,
                    &c->render->mesh,
                    dr_get_shader_rsc(GS_CHUNK),
                    {&c->render->render_data, vk::DEF_MESH_RENDER_DATA_SIZE});
                //}

                submit_mesh_shadow(
                    render_shadow_command_buffer,
                    &c->render->mesh,
                    dr_get_shader_rsc(GS_CHUNK_SHADOW),
                    {&c->render->render_data, vk::DEF_MESH_RENDER_DATA_SIZE});
            }
        }
    }
}

static void s_projectiles_gpu_sync_and_render(
    VkCommandBuffer render,
    VkCommandBuffer shadow,
    VkCommandBuffer transfer,
    vkph::state_t *state) {
    for (uint32_t i = 0; i < state->rocks.list.data_count; ++i) {
        vk::mesh_render_data_t data = {};
        data.color = vector4_t(0.0f);
        data.model = glm::translate(state->rocks.list[i].position) * glm::scale(vector3_t(0.2f));
        data.pbr_info.x = 0.5f;
        data.pbr_info.y = 0.5f;

        vk::begin_mesh_submission(render, dr_get_shader_rsc(GS_BALL));

        vk::submit_mesh(
            render,
            dr_get_mesh_rsc(GM_BALL),
            dr_get_shader_rsc(GS_BALL),
            {&data, vk::DEF_MESH_RENDER_DATA_SIZE});

        vk::submit_mesh_shadow(
            shadow,
            dr_get_mesh_rsc(GM_BALL),
            dr_get_shader_rsc(GS_BALL_SHADOW),
            {&data, vk::DEF_MESH_RENDER_DATA_SIZE});
    }
}

void dr_draw_game(
    VkCommandBuffer render,
    VkCommandBuffer transfer,
    VkCommandBuffer shadow,
    vkph::state_t *state) {
    const vk::eye_3d_info_t *eye_info = sc_get_eye_info();
    vk::swapchain_information_t swapchain_info = vk::get_swapchain_info();

    float aspect_ratio = (float)swapchain_info.width / (float)swapchain_info.height;

#if 0
    // Get view frustum for culling
    frustum_t frustum = {};
    create_frustum(
        &frustum,
        eye_info->position,
        eye_info->direction,
        eye_info->up,
        glm::radians(eye_info->fov / 2.0f),
        aspect_ratio,
        eye_info->near,
        eye_info->far);
#endif

    s_players_gpu_sync_and_render(render, shadow, transfer, state);
    s_chunks_gpu_sync_and_render(NULL, render, shadow, transfer, state);
    s_projectiles_gpu_sync_and_render(render, shadow, transfer, state);

    if (render != VK_NULL_HANDLE) {
        vk::render_environment(render);
    }
}

void dr_draw_premade_scene(
    VkCommandBuffer render,
    VkCommandBuffer transfer,
    fixed_premade_scene_t *scene) {
    vk::begin_mesh_submission(
        render,
        dr_get_shader_rsc(GS_BALL),
        dr_chunk_colors_g.chunk_color_set);

    scene->world_render_data.color = vector4_t(0.0f);

    vk::submit_mesh(
        render,
        &scene->world_mesh,
        dr_get_shader_rsc(GS_BALL),
        {&scene->world_render_data, vk::DEF_MESH_RENDER_DATA_SIZE});
}
