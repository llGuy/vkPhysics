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
    vec4 vs_light_positions[4];
    vec4 light_colors[4];
    vec4 vs_directional_light;

    mat4 shadow_view_projection;
    mat4 shadow_view;
    mat4 shadow_projection;

    vec2 light_screen_coord;
} u_lighting;

void main() {
    vec4 color = texture(u_diffuse, in_fs.uvs);
    vec4 raw_vs_normal = texture(u_gbuffer_normal, in_fs.uvs);
    
    vec3 vs_current = texture(u_gbuffer_position, in_fs.uvs).xyz;
    vec3 ws_current = (u_camera_transforms.inverse_view * vec4(vs_current, 1.0f)).xyz;

    vec4 previous = u_camera_transforms.previous_view_projection * vec4(ws_current, 1.0f);
    previous.xyz /= previous.w;
    previous.xy = previous.xy * 0.5f + 0.5f;

    float desired_fps = 60.0f;
    float current_fps = 1.0f / u_camera_transforms.dt;
    float scale = current_fps / desired_fps;
    vec2 blur = (previous.xy - in_fs.uvs) * scale;

    int num_samples = 8;
    for (int i = 1; i < num_samples; ++i) {
        vec2 offset = blur * (float(i) / float(num_samples - 1) - 0.5f);
        vec2 current_sample_position = in_fs.uvs + offset;
        color += texture(u_diffuse, current_sample_position);
    }

    color /= float(num_samples);
    color.a = 1.0f;

    out_final_color = color;

    const int SAMPLES = 40;
    const float DENSITY = 1.0;
    const float DECAY = 0.87;
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

    // Flip UV coord
    /*vec2 coord = -in_fs.uvs + vec2(1.0f);

    int ghosts = 5;
    float dispersal = 0.6f;

    vec2 texel_size = 1.0f / vec2(textureSize(u_bright, 0));

    vec2 ghost_vec = (vec2(0.5f) - coord) * dispersal;

    vec4 result = vec4(0.0f);
    for (int i = 0; i < ghosts; ++i) {
        vec2 offset = fract(coord + ghost_vec * float(i));

        float weight = length(vec2(0.5f) - offset) / length(vec2(0.5f));
        
        result += texture(u_bright, offset) * weight;
    }

    out_final_color = result;
    out_final_color *= ;*/
    
    //out_final_color = texture(u_bright, in_fs.uvs);
}
