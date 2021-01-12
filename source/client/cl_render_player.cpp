#include <app.hpp>
#include "cl_main.hpp"
#include "cl_game_predict.hpp"
#include "cl_render.hpp"
#include "vk.hpp"
#include <vkph_physics.hpp>

namespace cl {

static bool is_in_first_person = 0;

static vk::mesh_t player_mesh;
static vk::mesh_t ball_mesh;
static vk::mesh_t merged_mesh;

static object_shaders_t player_shaders;
static object_shaders_t ball_shaders;

/*
  These two are required when rendering the transition between player and ball
  (or vice-versa).
 */
static object_shaders_t merged_player_shaders;
static object_shaders_t merged_ball_shaders;

static struct {
    vk::skeleton_t player_sk;
    vk::animation_cycles_t player_cyc;
} animations;

void init_player_render_resources() {
    { // Load the animation data for the player "person" mesh
        animations.player_sk.load("assets/models/player.skeleton");
        animations.player_cyc.load("assets/models/player.animations.link", "assets/models/player.animations");
    }

    vk::shader_binding_info_t player_sbi, ball_sbi, merged_sbi;

    {// Create meshes also for transition effect between both models
        vk::mesh_loader_t skeletal_mesh_ld = {};
        skeletal_mesh_ld.load_external("assets/models/player.mesh");

        vk::mesh_loader_t ball_mesh_ld = {};
        ball_mesh_ld.load_external("assets/models/ball.mesh");
        //ball_mesh_ld.load_sphere();

        vk::merged_mesh_info_t info = {};
        info.dst_sk = &player_mesh;
        info.sk_loader = &skeletal_mesh_ld;
        info.dst_sk_sbi = &player_sbi;
        info.dst_st = &ball_mesh;
        info.st_loader = &ball_mesh_ld;
        info.dst_st_sbi = &ball_sbi;
        info.dst_merged = &merged_mesh;
        info.dst_merged_sbi = &merged_sbi;

        vk::create_merged_mesh(&info);
    }

    vk::shader_create_info_t sc_info;

    { // Create shaders for the ball mesh
        const char *static_shader_paths[] = {
            "shaders/SPV/mesh.vert.spv", "shaders/SPV/mesh.geom.spv", "shaders/SPV/mesh.frag.spv"
        };

        const char *static_shadow_shader_paths[] = {
            "shaders/SPV/mesh_shadow.vert.spv", "shaders/SPV/shadow.frag.spv"
        };

        sc_info.binding_info = &ball_sbi;
        sc_info.shader_paths = static_shader_paths;
        sc_info.shader_flags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        sc_info.cull_mode = VK_CULL_MODE_NONE;
        sc_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        ball_shaders.albedo = vk::create_mesh_shader_color(&sc_info, vk::MT_STATIC);

        sc_info.shader_paths = static_shadow_shader_paths;
        sc_info.shader_flags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        ball_shaders.shadow = vk::create_mesh_shader_shadow(&sc_info, vk::MT_STATIC);
    }

    { // Create shaders for the "person" mesh
        const char *player_shader_paths[] = {
            "shaders/SPV/skeletal.vert.spv", "shaders/SPV/skeletal.geom.spv", "shaders/SPV/skeletal.frag.spv"
        };

        const char *player_shadow_shader_paths[] = {
            "shaders/SPV/skeletal_shadow.vert.spv", "shaders/SPV/shadow.frag.spv"
        };

        sc_info.binding_info = &player_sbi;
        sc_info.shader_paths = player_shader_paths;
        sc_info.shader_flags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        sc_info.cull_mode = VK_CULL_MODE_NONE;
        sc_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        player_shaders.albedo = vk::create_mesh_shader_color(&sc_info, vk::MT_ANIMATED);

        sc_info.shader_paths = player_shadow_shader_paths;
        sc_info.shader_flags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        player_shaders.shadow = vk::create_mesh_shader_shadow(&sc_info, vk::MT_ANIMATED);
    }

    { // Create shaders for the transition effect between person and ball
        const char *merged_shader_paths[] = {
            "shaders/SPV/morph.vert.spv", "shaders/SPV/morph_ball.geom.spv", "shaders/SPV/morph.frag.spv"
        };

        const char *merged_shadow_shader_paths[] = {
            "shaders/SPV/morph.vert.spv", "shaders/SPV/morph_ball_shadow.geom.spv", "shaders/SPV/shadow.frag.spv"
        };

        sc_info.binding_info = &merged_sbi;
        sc_info.shader_paths = merged_shader_paths;
        sc_info.shader_flags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        sc_info.cull_mode = VK_CULL_MODE_NONE;
        sc_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY;

        merged_ball_shaders.albedo = vk::create_mesh_shader_color(&sc_info, vk::MT_ANIMATED | vk::MT_MERGED_MESH);

#if 1
        sc_info.shader_paths = merged_shadow_shader_paths;
        merged_ball_shaders.shadow = vk::create_mesh_shader_shadow(&sc_info, vk::MT_ANIMATED | vk::MT_MERGED_MESH);
#endif

        merged_shader_paths[1] = "shaders/SPV/morph_dude.geom.spv";
        merged_shadow_shader_paths[1] = "shaders/SPV/morph_dude_shadow.geom.spv";

        sc_info.shader_paths = merged_shader_paths;

        merged_player_shaders.albedo = vk::create_mesh_shader_color(&sc_info, vk::MT_ANIMATED | vk::MT_MERGED_MESH);

#if 1
        sc_info.shader_paths = merged_shadow_shader_paths;
        merged_player_shaders.shadow = vk::create_mesh_shader_shadow(&sc_info, vk::MT_ANIMATED | vk::MT_MERGED_MESH);
#endif
    }
}

bool &is_first_person() {
    return is_in_first_person;
}

player_render_t *init_player_render() {
    player_render_t *player_render = flmalloc<player_render_t>(1);
    memset(player_render, 0, sizeof(player_render_t));
    player_render->rolling_matrix = matrix4_t(1.0f);
    return player_render;
}

void init_player_animated_instance(vk::animated_instance_t *instance) {
    instance->init(&animations.player_sk, &animations.player_cyc);
}

bool is_animation_repeating(vkph::player_animated_state_t state) {
    static const bool map[] = {1, 1, 1, 0, 0, 1, 1, 0, 1, 1};
    
    return map[(uint32_t)state];
}

static void s_render_person(
    VkCommandBuffer render,
    VkCommandBuffer shadow,
    VkCommandBuffer transfer,
    vkph::player_t *p) {
    if (p->render->animations.next_bound_cycle != p->animated_state)
        p->render->animations.switch_to_cycle(p->animated_state, 0);

    p->render->animations.interpolate_joints(
        app::g_delta_time,
        is_animation_repeating((vkph::player_animated_state_t)p->animated_state));

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

    vk::submit_skeletal_mesh(render, &player_mesh, &player_shaders.albedo, rdata, &p->render->animations);
    vk::submit_skeletal_mesh_shadow(shadow, &player_mesh, &player_shaders.shadow, rdata, &p->render->animations);
}

static void s_render_ball(
    VkCommandBuffer render,
    VkCommandBuffer shadow,
    vkph::player_t *p) {
    p->render->rotation_speed = p->frame_displacement / calculate_sphere_circumference(vkph::PLAYER_SCALE) * 360.0f;
    p->render->rotation_angle += p->render->rotation_speed;

    if (p->render->rotation_angle > 360.0f)
        p->render->rotation_angle -= 360.0f;

    // if (glm::dot(p->ws_velocity, p->ws_velocity) > 0.0001f) {
    if (p->frame_displacement > 0.0f) {
        vector3_t cross = glm::cross(glm::normalize(p->ws_velocity), p->ws_up_vector);
        vector3_t right = glm::normalize(cross);
        matrix4_t rolling_rotation = glm::rotate(glm::radians(p->render->rotation_angle), -right);
        p->render->rolling_matrix = rolling_rotation;
    }


    if (app::get_raw_input()->buttons[app::BT_P].state == app::button_state_t::BS_DOWN) {
        // printf("%s\n", glm::to_string().c_str());
        printf("Still rolling? -> %s (while frame displacement is %f)\n", glm::to_string(p->render->rolling_matrix).c_str(), p->frame_displacement);
    }

    p->render->render_data.model = glm::translate(p->ws_position) * p->render->rolling_matrix * glm::scale(vector3_t(vkph::PLAYER_SCALE));

    buffer_t rdata = {&p->render->render_data, vk::DEF_MESH_RENDER_DATA_SIZE};

    vk::begin_mesh_submission(render, &ball_shaders.albedo);
    vk::submit_mesh(render, &ball_mesh, &ball_shaders.albedo, rdata);
    vk::submit_mesh_shadow(shadow, &ball_mesh, &ball_shaders.shadow, {rdata});
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

    p->render->animations.interpolate_joints(app::g_delta_time, is_animation_repeating((vkph::player_animated_state_t)p->animated_state));
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
            &merged_mesh,
            &merged_ball_shaders.albedo,
            {&render_data, sizeof(render_data)},
            &p->render->animations);

        submit_skeletal_mesh_shadow(
            render_shadow_command_buffer,
            &merged_mesh,
            &merged_ball_shaders.shadow,
            {&render_data, sizeof(render_data)},
            &p->render->animations);
    }
    else {
        // Need to render transition from person to ball
        submit_skeletal_mesh(
            render_command_buffer,
            &merged_mesh,
            &merged_player_shaders.albedo,
            {&render_data, sizeof(render_data)},
            &p->render->animations);

        submit_skeletal_mesh_shadow(
            render_shadow_command_buffer,
            &merged_mesh,
            &merged_player_shaders.shadow,
            {&render_data, sizeof(render_data)},
            &p->render->animations);
    }
}

// When skeletal animation is implemented, this function will do stuff like handle that
void players_gpu_sync_and_render(
    VkCommandBuffer render_command_buffer,
    VkCommandBuffer render_shadow_command_buffer,
    VkCommandBuffer transfer_command_buffer,
    vkph::state_t *state) {
    vk::insert_debug(render_command_buffer, "Render Players Region", vector4_t(0.8f, 0.4f, 0.1f, 1.0f));
    vk::insert_debug(render_shadow_command_buffer, "Render Shadow Players Region", vector4_t(0.8f, 0.4f, 0.1f, 1.0f));
    vk::insert_debug(transfer_command_buffer, "Transfer Players Region", vector4_t(0.8f, 0.4f, 0.1f, 1.0f));

    for (uint32_t i = 0; i < state->players.data_count; ++i) {
        vkph::player_t *p = state->get_player(i);
        if (p) {
            if (p->flags.is_alive) {
                if (!p->render) {
                    p->render = init_player_render();
                }

                p->render->render_data.color = team_color_to_v4((vkph::team_color_t)p->flags.team_color);
                p->render->render_data.pbr_info.x = 0.1f;
                p->render->render_data.pbr_info.y = 0.1f;

                if ((int32_t)i == (int32_t)get_local_player(state)) {
                    if (!is_first_person()) {
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

}
