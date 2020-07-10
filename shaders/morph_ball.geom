#version 450

layout(triangles_adjacency) in;
layout(triangle_strip, max_vertices = 3) out;

// First 3 are from the first mesh (player), second 3 are from the second mesh (balll)
layout(location = 0) in VS_DATA {
    vec3 position;
    vec3 normal;
    vec4 weights;
    ivec4 joint_ids;
} in_gs[];

layout(location = 0) out GS_DATA {
    vec3 position;
    vec3 normal;
} out_gs;

layout(push_constant) uniform push_constant_t {
    mat4 model1;
    mat4 model2;
    vec4 color0;
    vec4 pbr_info;
    int texture_id;

    float progression;
} u_push_constant;

layout(set = 0, binding = 0) uniform camera_transforms_t {
    mat4 projection;
    mat4 view;
    mat4 inverse_view;
    mat4 view_projection;
    vec4 frustum;
    vec4 view_direction;
    mat4 previous_view_projection;
} u_camera_transforms;

layout(set = 1, binding = 0) uniform joint_transforms_t {
    mat4 transforms[25];
} joints;

#define MAX_WEIGHTS 4

mat4 rotate(
    float angle,
    vec3 axis) {
    float a = angle;
    float c = cos(a);
    float s = sin(a);

    axis = normalize(axis);
    vec3 temp = ((1.0f - c) * axis);

    mat4 rotate = mat4(1.0f);
    rotate[0][0] = c + temp[0] * axis[0];
    rotate[0][1] = temp[0] * axis[1] + s * axis[2];
    rotate[0][2] = temp[0] * axis[2] - s * axis[1];

    rotate[1][0] = temp[1] * axis[0] - s * axis[2];
    rotate[1][1] = c + temp[1] * axis[1];
    rotate[1][2] = temp[1] * axis[2] + s * axis[0];

    rotate[2][0] = temp[2] * axis[0] + s * axis[1];
    rotate[2][1] = temp[2] * axis[1] - s * axis[0];
    rotate[2][2] = c + temp[2] * axis[2];

    return rotate;
}

vec3 get_normal(vec3 a, vec3 b, vec3 c) {
    vec3 diff_world_pos1 = normalize(b - a);
    vec3 diff_world_pos2 = normalize(c - b);
    return(normalize(cross(diff_world_pos2, diff_world_pos1)));
}

void main() {
    mat4 rotate_only_model = u_push_constant.model1;
    rotate_only_model[3][0] = 0.0f;
    rotate_only_model[3][1] = 0.0f;
    rotate_only_model[3][2] = 0.0f;

    mat4 rotate_only_model2 = u_push_constant.model2;
    rotate_only_model2[3][0] = 0.0f;
    rotate_only_model2[3][1] = 0.0f;
    rotate_only_model2[3][2] = 0.0f;

    // Calculate the vertices for the dude (in model space)
    vec4 dude_ms_positions[3];
    vec4 dude_ms_normals[3];

    vec4 dude_vs_positions[3];
    vec4 dude_vs_normals[3];

    for (int i = 0; i < 3; ++i) {
        mat4 identity = mat4(
            1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            0, 0, 0, 1);

        vec4 accumulated_local = vec4(0);
        vec4 accumulated_normal = vec4(0);

        for (int w = 0; w < MAX_WEIGHTS; ++w) {
            vec4 original_pos = vec4(in_gs[i].position, 1.0f);

            mat4 joint = joints.transforms[in_gs[i].joint_ids[w]];
                
            vec4 pose_position = joint * original_pos;

            accumulated_local += pose_position * in_gs[i].weights[w];

            vec4 world_normal = joint * vec4(in_gs[i].normal, 0.0f);
            accumulated_normal += world_normal * in_gs[i].weights[w];
        }

        mat4 rotation_correction = rotate(radians(-90.0f), vec3(1, 0, 0));

        dude_ms_positions[i] = rotate_only_model * rotation_correction * accumulated_local;
        dude_ms_normals[i] = rotate_only_model * rotation_correction * vec4(accumulated_normal.xyz, 0.0f);

        dude_vs_positions[i] = u_camera_transforms.view * dude_ms_positions[i];
        dude_vs_normals[i] = u_camera_transforms.view * dude_ms_normals[i];
    }

    vec4 ball_ms_positions[3];
    vec4 ball_ms_normals[3];
    vec4 ball_vs_positions[3];
    vec4 ball_vs_normals[3];

    vec3 ball_normal = vec3(vec4(get_normal(in_gs[3].position, in_gs[4].position, in_gs[5].position), 0.0));

    for (int i = 0; i < 3; ++i) {
        ball_ms_positions[i] = rotate_only_model2 * vec4(in_gs[i + 3].position, 1.0f);
        ball_ms_normals[i] = rotate_only_model2 * vec4(ball_normal, 0.0f);

        ball_vs_positions[i] = u_camera_transforms.view * ball_ms_positions[i];
        ball_vs_normals[i] = u_camera_transforms.view * ball_ms_normals[i];
    }

    /* float dude_triangle_final_distance = 0.7f; */
    /* float ball_triangle_final_distance = 1.5f; */
    float dude_triangle_final_distance = 0.0f;
    float ball_triangle_final_distance = 0.0f;
    /* const float extrude_time = 0.25f; */
    /* const float displacement_time = 0.5f; */
    /* const float fallback_time = 0.25f; */
    /* const float extrude_time = 0.00001f; */
    /* const float displacement_time = 0.999f; */
    /* const float fallback_time = 0.00099f; */

    /* if (u_push_constant.progression < extrude_time) { */
    /*     vec3 model_center = (u_camera_transforms.view * vec4(u_push_constant.model1[3][0], u_push_constant.model1[3][1], u_push_constant.model1[3][2], 1.0f)).xyz; */

    /*     vec3 a_to_b = ball_vs_positions[1].xyz - ball_vs_positions[0].xyz; */
    /*     a_to_b /= 2.0f; */
    /*     vec3 c_to_center = (a_to_b + ball_vs_positions[0].xyz) - ball_vs_positions[2].xyz; */
    /*     vec3 center = ball_vs_positions[2].xyz + c_to_center / 2.0f; */
    /*     vec3 outer_vector = normalize(center - vec3(model_center)) * 0.5; */

    /*     // Get triangles out of the model */
    /*     for (int i = 0; i < 3; ++i) { */
    /*         vec3 normal = normalize(ball_vs_normals[i].xyz); */

    /*         vec3 final_vs_position = ball_vs_positions[i].xyz + outer_vector * ball_triangle_final_distance * u_push_constant.progression / extrude_time; */
    /*         gl_Position = u_camera_transforms.projection * vec4(final_vs_position, 1.0f); */

    /*         out_gs.position = final_vs_position.xyz; */
    /*         // out_gs.normal = ball_vs_normals[i].xyz; */
    /*         out_gs.normal = normal; */

    /*         EmitVertex(); */
    /*     } */
    /* } */
    if (u_push_constant.progression > 0.00001f) {
        float progress = u_push_constant.progression;

        vec3 model_center = (u_camera_transforms.view * vec4(u_push_constant.model2[3][0], u_push_constant.model2[3][1], u_push_constant.model2[3][2], 1.0f)).xyz;

        vec3 a_to_b = ball_vs_positions[1].xyz - ball_vs_positions[0].xyz;
        a_to_b /= 2.0f;
        vec3 c_to_center = (a_to_b + ball_vs_positions[0].xyz) - ball_vs_positions[2].xyz;
        vec3 center = ball_vs_positions[2].xyz + c_to_center / 2.0f;
        vec3 outer_vector = normalize(center - vec3(model_center)) * 0.5;

        vec3 model_center2 = (u_camera_transforms.view * vec4(u_push_constant.model1[3][0], u_push_constant.model1[3][1], u_push_constant.model1[3][2], 1.0f)).xyz;

        vec3 a_to_b2 = dude_vs_positions[1].xyz - dude_vs_positions[0].xyz;
        a_to_b2 /= 2.0f;
        vec3 c_to_center2 = (a_to_b2 + dude_vs_positions[0].xyz) - dude_vs_positions[2].xyz;
        vec3 center2 = dude_vs_positions[2].xyz + c_to_center2 / 2.0f;
        vec3 outer_vector2 = normalize(center2 - vec3(model_center2)) * 0.5;

        for (int i = 0; i < 3; ++i) {
            vec3 normal = normalize(ball_vs_normals[i].xyz);

            vec3 previous_vs_position = ball_vs_positions[i].xyz + outer_vector * ball_triangle_final_distance;
            vec3 next_position = dude_vs_positions[i].xyz + outer_vector2 *dude_triangle_final_distance;

            vec3 final_position = previous_vs_position + (next_position - previous_vs_position) * progress;

            vec3 previous_normal = ball_vs_normals[i].xyz;
            vec3 next_normal = dude_vs_normals[i].xyz;

            vec3 current_normal = normalize(previous_normal + (next_normal - previous_normal) * progress);

            gl_Position = u_camera_transforms.projection * vec4(final_position, 1.0f);

            out_gs.position = final_position.xyz;
            out_gs.normal = current_normal;

            EmitVertex();
        }
    }
    /* else if (u_push_constant.progression < 1.0f) { */
    /*     float progress = u_push_constant.progression - (extrude_time + displacement_time); */
    /*     progress /= fallback_time; */

    /*     vec3 model_center = (u_camera_transforms.view * vec4(u_push_constant.model1[3][0], u_push_constant.model1[3][1], u_push_constant.model1[3][2], 1.0f)).xyz; */

    /*     vec3 a_to_b = dude_vs_positions[1].xyz - dude_vs_positions[0].xyz; */
    /*     a_to_b /= 2.0f; */
    /*     vec3 c_to_center = (a_to_b + dude_vs_positions[0].xyz) - dude_vs_positions[2].xyz; */
    /*     vec3 center = dude_vs_positions[2].xyz + c_to_center / 2.0f; */
    /*     vec3 outer_vector = normalize(center - vec3(model_center)) * 0.5; */

    /*     // Get triangles out of the model */
    /*     for (int i = 0; i < 3; ++i) { */
    /*         vec3 normal = normalize(dude_vs_normals[i].xyz); */

    /*         vec3 final_vs_position = dude_vs_positions[i].xyz + outer_vector * dude_triangle_final_distance; */
    /*         final_vs_position += (dude_vs_positions[i].xyz - final_vs_position) * progress; */
    /*         gl_Position = u_camera_transforms.projection * vec4(final_vs_position, 1.0f); */

    /*         out_gs.position = final_vs_position.xyz; */
    /*         // out_gs.normal = ball_vs_normals[i].xyz; */
    /*         out_gs.normal = normal; */

    /*         EmitVertex(); */
    /*     } */
    /* } */
    else {
#if 1
        for (int i = 0; i < 3; ++i) {
            gl_Position = u_camera_transforms.projection * u_camera_transforms.view * rotate_only_model2 * vec4(in_gs[i + 3].position, 1.0f);

            out_gs.position = ball_vs_positions[i].xyz;
            out_gs.normal = normalize(ball_vs_normals[i].xyz);

            EmitVertex();
        }
    }
    #endif
    #if 0
    #endif
}
