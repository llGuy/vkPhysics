#include "common/containers.hpp"
#include "world.hpp"
#include "w_internal.hpp"

static scene_rendering_t scene_rendering;

static void s_create_player_shaders_and_meshes() {
    // Load the animation data for the player "person" mesh
    load_skeleton(&scene_rendering.player_skeleton, "assets/models/player.skeleton");
    load_animation_cycles(&scene_rendering.player_cycles, "assets/models/player.animations.link", "assets/models/player.animations");

    shader_binding_info_t player_sbi, ball_sbi, merged_sbi;

    // Create meshes also for transition effect between both models
    create_player_merged_mesh(
        &scene_rendering.player_mesh, &player_sbi,
        &scene_rendering.player_ball_mesh, &ball_sbi,
        &scene_rendering.merged_mesh, &merged_sbi);

    // Create shaders for the ball mesh
    const char *static_shader_paths[] = {
        "shaders/SPV/mesh.vert.spv",
        "shaders/SPV/mesh.geom.spv",
        "shaders/SPV/mesh.frag.spv"
    };

    scene_rendering.player_ball_shader = create_mesh_shader_color(
        &ball_sbi,
        static_shader_paths,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        VK_CULL_MODE_NONE,
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        MT_STATIC);

    // Create shaders for the "person" mesh
    const char *player_shader_paths[] = {
        "shaders/SPV/skeletal.vert.spv",
        "shaders/SPV/skeletal.geom.spv",
        "shaders/SPV/skeletal.frag.spv"
    };

    scene_rendering.player_shader = create_mesh_shader_color(
        &player_sbi,
        player_shader_paths,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        VK_CULL_MODE_NONE, 
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        MT_ANIMATED);

    // Create shaders for the transition effect between person and ball
    const char *merged_shader_paths[] = {
        "shaders/SPV/morph.vert.spv",
        "shaders/SPV/morph_ball.geom.spv",
        "shaders/SPV/morph.frag.spv",
    };

    scene_rendering.merged_shader_ball = create_mesh_shader_color(
        &merged_sbi,
        merged_shader_paths,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        VK_CULL_MODE_NONE, 
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY,
        MT_ANIMATED | MT_MERGED_MESH);

    merged_shader_paths[1] = "shaders/SPV/morph_dude.geom.spv";

    scene_rendering.merged_shader_player = create_mesh_shader_color(
        &merged_sbi,
        merged_shader_paths,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        VK_CULL_MODE_NONE, 
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY,
        MT_ANIMATED | MT_MERGED_MESH);
}

static void s_create_chunk_shaders() {
    mesh_t chunk_mesh_prototype = {};
    push_buffer_to_mesh(
        BT_VERTEX,
        &chunk_mesh_prototype);

    shader_binding_info_t binding_info = create_mesh_binding_info(
        &chunk_mesh_prototype);

    const char *shader_paths[] = {
        "shaders/SPV/chunk_mesh.vert.spv",
        "shaders/SPV/chunk_mesh.geom.spv",
        "shaders/SPV/chunk_mesh.frag.spv"
    };
    
    scene_rendering.chunk_shader = create_mesh_shader_color(
        &binding_info,
        shader_paths,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        VK_CULL_MODE_FRONT_BIT,
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        MT_STATIC | MT_PASS_EXTRA_UNIFORM_BUFFER);

    scene_rendering.chunk_color_data.pointer_position = vector4_t(0.0f);
    scene_rendering.chunk_color_data.pointer_color = vector4_t(0.0f);
    scene_rendering.chunk_color_data.pointer_radius = 0.0f;

    scene_rendering.chunk_color_data_buffer = create_gpu_buffer(
        sizeof(chunk_color_data_t),
        &scene_rendering.chunk_color_data,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    scene_rendering.chunk_color_data_buffer_set = create_buffer_descriptor_set(
        scene_rendering.chunk_color_data_buffer.buffer,
        sizeof(chunk_color_data_t),
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
}

void w_create_shaders_and_meshes() {
    s_create_player_shaders_and_meshes();
    s_create_chunk_shaders();
}

void w_player_animated_instance_init(
    animated_instance_t *instance) {
    animated_instance_init(instance, &scene_rendering.player_skeleton, &scene_rendering.player_cycles);
}

bool w_animation_is_repeat(
    player_animated_state_t state) {
    static const bool map[] = {1, 1, 1, 0, 0, 1, 1, 0, 1, 1};
    
    return map[(uint32_t)state];
}

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

    interpolate_joints(&p->render->animations, logic_delta_time(), w_animation_is_repeat((player_animated_state_t)p->animated_state));
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
        scene_rendering.player_cycles.rotation *
        glm::scale(PLAYER_SCALE) *
        scene_rendering.player_cycles.scale;

    submit_skeletal_mesh(
        render_command_buffer,
        &scene_rendering.player_mesh,
        &scene_rendering.player_shader,
        &p->render->render_data,
        &p->render->animations);
}

static void s_render_ball(
    VkCommandBuffer render_command_buffer,
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

    begin_mesh_submission(render_command_buffer, &scene_rendering.player_ball_shader);
    submit_mesh(
        render_command_buffer,
        &scene_rendering.player_ball_mesh,
        &scene_rendering.player_ball_shader,
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

    interpolate_joints(&p->render->animations, logic_delta_time(), w_animation_is_repeat((player_animated_state_t)p->animated_state));
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
        scene_rendering.player_cycles.rotation *
        glm::scale(PLAYER_SCALE) *
        scene_rendering.player_cycles.scale;

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
    render_data.progression = p->render->shape_animation_time / SHAPE_SWITCH_ANIMATION_TIME;

    if (p->flags.interaction_mode == PIM_STANDING) {
        // Need to render transition from ball to person
        submit_skeletal_mesh(
            render_command_buffer,
            &scene_rendering.merged_mesh,
            &scene_rendering.merged_shader_ball,
            &render_data,
            sizeof(render_data),
            &p->render->animations);
    }
    else {
        // Need to render transition from person to ball
        submit_skeletal_mesh(
            render_command_buffer,
            &scene_rendering.merged_mesh,
            &scene_rendering.merged_shader_player,
            &render_data,
            sizeof(render_data),
            &p->render->animations);
    }
}

// When skeletal animation is implemented, this function will do stuff like handle that
void w_players_gpu_sync_and_render(
    VkCommandBuffer render_command_buffer,
    VkCommandBuffer render_shadow_command_buffer,
    VkCommandBuffer transfer_command_buffer) {
    for (uint32_t i = 0; i < get_player_count(); ++i) {
        player_t *p = get_player(i);
        if (p) {
            if (p->flags.alive_state == PAS_ALIVE) {
                if (!p->render) {
                    w_player_render_init(p);
                }

                p->render->render_data.color = vector4_t(1.0f);
                p->render->render_data.pbr_info.x = 0.1f;
                p->render->render_data.pbr_info.y = 0.1f;

                if ((int32_t)i == (int32_t)w_local_player_index()) {
                    if (p->switching_shapes) {
                        // Render transition
                        s_render_transition(render_command_buffer, transfer_command_buffer, p);
                    }
                    else if (p->flags.interaction_mode == PIM_STANDING) {
                        s_render_person(render_command_buffer, transfer_command_buffer, p);
                    }
                    else {
                        s_render_ball(render_command_buffer, p);
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
                        s_render_ball(render_command_buffer, p);
                    } 
                }
            }
        }
    }
}

void gpu_sync_world(
    bool in_startup,
    VkCommandBuffer render_command_buffer,
    VkCommandBuffer render_shadow_command_buffer,
    VkCommandBuffer transfer_command_buffer) {
    if (in_startup) {
        // Render the startup world (which was loaded in from a file)
        w_render_startup_world(render_command_buffer);

        if (render_command_buffer != VK_NULL_HANDLE) {
            render_environment(render_command_buffer);
        }
    }
    else {
        // Render the actual world
        w_players_gpu_sync_and_render(
            render_command_buffer,
            render_shadow_command_buffer,
            transfer_command_buffer);

        w_chunk_gpu_sync_and_render(
            render_command_buffer,
            transfer_command_buffer);
    
        if (render_command_buffer != VK_NULL_HANDLE) {
            render_environment(render_command_buffer);
        }
    }
}

static player_t *s_get_camera_bound_player() {
    player_t *player = w_get_local_player();

    if (!player) {
        player = w_get_spectator();
    }
    else if (player->flags.alive_state != PAS_ALIVE) {
        player = w_get_spectator();
    }

    return player;
}

static void s_calculate_3rd_person_position_and_direction(
    player_t *player,
    vector3_t *position,
    vector3_t *direction) {
    *position = player->ws_position - player->ws_view_direction * player->camera_distance.current * PLAYER_SCALE.x;
    *position += player->current_camera_up * PLAYER_SCALE * 2.0f;

    if (player->flags.interaction_mode == PIM_STANDING && player->flags.moving) {
        // Add view bobbing
        static float angle = 0.0f;
        static float right = 0.0f;
        angle += logic_delta_time() * 10.0f;
        right += logic_delta_time() * 5.0f;
        angle = fmod(angle, glm::radians(360.0f));
        right = fmod(right, glm::radians(360.0f));

        vector3_t view_dest = *position + *direction;

        vector3_t right_vec = glm::normalize(glm::cross(player->ws_view_direction, player->current_camera_up));
        *position += player->current_camera_up * glm::sin(angle) * 0.004f + right_vec * glm::sin(right) * 0.002f;
        *direction = glm::normalize(view_dest - *position);
    }
}

eye_3d_info_t create_eye_info() {
    eye_3d_info_t info = {};
    
    player_t *player = s_get_camera_bound_player();

    vector3_t view_direction = player->ws_view_direction;

    s_calculate_3rd_person_position_and_direction(
        player,
        &info.position,
        &view_direction);
    
    info.direction = view_direction;
    info.up = player->current_camera_up;

    info.fov = player->camera_fov.current;
    info.near = 0.01f;
    info.far = 10000.0f;
    info.dt = surface_delta_time();

    return info;
}

lighting_info_t create_lighting_info() {
    lighting_info_t info = {};
    
    info.ws_directional_light = vector4_t(0.1f, 0.422f, 0.714f, 0.0f);
#if 0
    info.ws_light_positions[0] = vector4_t(player.position, 1.0f);
    info.ws_light_directions[0] = vector4_t(player.direction, 0.0f);
    info.light_colors[0] = vector4_t(100.0f);
#endif
    info.lights_count = 0;

#if 0
    for (uint32_t i = 0; i < world.players.data_count; ++i) {
        if (world.players[i]->flags.flashing_light) {
            info.ws_light_positions[info.lights_count] = vector4_t(world.players[i]->ws_position + world.players[i]->ws_up_vector, 1.0f);
            info.ws_light_directions[info.lights_count] = vector4_t(world.players[i]->ws_view_direction, 0.0f);
            info.light_colors[info.lights_count++] = vector4_t(100.0f);
        }
    }
#endif

    return info;
}

void w_chunk_gpu_sync_and_render(
    VkCommandBuffer render_command_buffer,
    VkCommandBuffer transfer_command_buffer) {
    const uint32_t max_chunks_loaded_per_frame = 10;
    uint32_t chunks_loaded = 0;

    terraform_package_t *current_terraform_package = w_get_local_current_terraform_package();

    if (current_terraform_package->ray_hit_terrain) {
        scene_rendering.chunk_color_data.pointer_radius = 3.0f;
    }
    else {
        scene_rendering.chunk_color_data.pointer_radius = 0.0f;
    }

    scene_rendering.chunk_color_data.pointer_position = vector4_t(current_terraform_package->ws_position, 1.0f);
    scene_rendering.chunk_color_data.pointer_color = vector4_t(0.0f, 1.0f, 1.0f, 1.0f);

    update_gpu_buffer(
        transfer_command_buffer,
        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
        0,
        sizeof(chunk_color_data_t),
        &scene_rendering.chunk_color_data,
        &scene_rendering.chunk_color_data_buffer);

    begin_mesh_submission(
        render_command_buffer,
        &scene_rendering.chunk_shader,
        scene_rendering.chunk_color_data_buffer_set);

    uint32_t chunk_count;
    chunk_t **chunks = get_active_chunks(&chunk_count);
    uint8_t surface_level = CHUNK_SURFACE_LEVEL;

    for (uint32_t i = 0; i < chunk_count; ++i) {
        chunk_t *c = chunks[i];
        if (c) {
            if (c->flags.has_to_update_vertices && chunks_loaded < max_chunks_loaded_per_frame/* && !chunks.wait_mesh_update*/) {
                c->flags.has_to_update_vertices = 0;
                // Update chunk mesh and put on GPU + send to command buffer
                // TODO:
                w_update_chunk_mesh(
                    transfer_command_buffer,
                    surface_level,
                    c);

                ++chunks_loaded;
            }
        
            if (c->flags.active_vertices) {
                submit_mesh(
                    render_command_buffer,
                    &c->render->mesh,
                    &scene_rendering.chunk_shader,
                    &c->render->render_data);
            }
        }
    }
}

void w_render_startup_world(
    VkCommandBuffer render_command_buffer) {
    startup_screen_t *startup_data = w_get_startup_screen_data();

    begin_mesh_submission(
        render_command_buffer,
        &scene_rendering.chunk_shader,
        scene_rendering.chunk_color_data_buffer_set);

    submit_mesh(
        render_command_buffer,
        &startup_data->world_mesh,
        &scene_rendering.chunk_shader,
        &startup_data->world_render_data);
}
