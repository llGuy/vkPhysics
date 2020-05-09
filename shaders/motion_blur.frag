#version 450

layout(location = 0) in VS_DATA {
    vec2 uvs;
} in_fs;

layout(location = 0) out vec4 out_final_color;

layout(binding = 0, set = 0) uniform sampler2D u_gbuffer_albedo;
layout(binding = 1, set = 0) uniform sampler2D u_gbuffer_normal;
layout(binding = 2, set = 0) uniform sampler2D u_gbuffer_position;
layout(binding = 3, set = 0) uniform sampler2D u_gbuffer_sun;
// TODO: Get position from depth buffer - for now, use gbuffer position as testcase
layout(binding = 4, set = 0) uniform sampler2D u_gbuffer_depth;

layout(binding = 0, set = 1) uniform camera_transforms_t {
    mat4 projection;
    mat4 view;
    mat4 inverse_view;
    mat4 view_projection;
    vec4 frustum;
    vec4 view_direction;
    mat4 previous_view_projection;
    float dt;
} u_camera_transforms;

layout(binding = 0, set = 2) uniform sampler2D u_diffuse;
layout(binding = 1, set = 2) uniform sampler2D u_bright;

layout(binding = 0, set = 3) uniform lighting_t {
    vec4 vs_light_positions[10];
    vec4 ws_light_positions[10];
    vec4 vs_light_directions[10];
    vec4 ws_light_directions[10];
    vec4 light_colors[10];
    vec4 vs_directional_light;
    vec4 ws_directional_light;

    mat4 shadow_view_projection;
    mat4 shadow_view;
    mat4 shadow_projection;

    vec2 light_screen_coord;
    
    int point_light_count;
} u_lighting;

void main() {
    vec4 color = texture(u_diffuse, in_fs.uvs);
    vec4 raw_vs_normal = texture(u_gbuffer_normal, in_fs.uvs);
    
    vec3 vs_current = texture(u_gbuffer_position, in_fs.uvs).xyz;
    // Converts directly from current view space, to previous projection space
    vec4 previous = u_camera_transforms.previous_view_projection * vec4(vs_current, 1.0f);
    previous.xyz /= previous.w;
    previous.xy = previous.xy * 0.5f + 0.5f;

    float desired_fps = 100.0f;
    float current_fps = 1.0f / u_camera_transforms.dt;
    float scale = current_fps / desired_fps;
    vec2 blur = (previous.xy - in_fs.uvs);// * scale;

    int num_samples = 1;
    // for (int i = 1; i < num_samples; ++i) {
    //     vec2 offset = blur * (float(i) / float(num_samples - 1) - 0.5f);
    //     vec2 current_sample_position = in_fs.uvs + offset;
    //     color += texture(u_diffuse, current_sample_position);
    // }

    color /= float(num_samples);
    color.a = 1.0f;

    out_final_color = color;

    out_final_color = color;

    const int SAMPLES = 50;
    const float DENSITY = 1.0;
    const float DECAY = 0.91;
    const float WEIGHT = 0.03;

    vec2 blur_vector = (u_lighting.light_screen_coord - in_fs.uvs) * (1.0 / float(SAMPLES));
    vec2 current_uvs = in_fs.uvs;

    float illumination_decay = 1.0;
    
    for (int i = 0; i < SAMPLES; ++i) {
        current_uvs += blur_vector;

        vec4 current_color = texture(u_gbuffer_sun, current_uvs);

        current_color *= illumination_decay * WEIGHT;

        out_final_color += current_color;

        illumination_decay *= DECAY;
    }
}
