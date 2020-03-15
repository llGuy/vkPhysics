#version 450

layout(location = 0) in VS_DATA {
    vec2 uvs;
} in_fs;

layout(location = 0) out vec4 out_final_color;

layout(binding = 0, set = 0) uniform sampler2D u_gbuffer_albedo;
layout(binding = 1, set = 0) uniform sampler2D u_gbuffer_normal;
layout(binding = 2, set = 0) uniform sampler2D u_gbuffer_position;
// TODO: Get position from depth buffer - for now, use gbuffer position as testcase
layout(binding = 3, set = 0) uniform sampler2D u_gbuffer_depth;

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

void main() {
    vec4 color = texture(u_diffuse, in_fs.uvs);
    vec4 raw_vs_normal = texture(u_gbuffer_normal, in_fs.uvs);
    
    vec3 vs_current = texture(u_gbuffer_position, in_fs.uvs).xyz;
    vec3 ws_current = (u_camera_transforms.inverse_view * vec4(vs_current, 1.0f)).xyz;

    vec4 previous = u_camera_transforms.previous_view_projection * vec4(ws_current, 1.0f);
    previous.xyz /= previous.w;
    previous.xy = previous.xy * 0.5f + 0.5f;

    float desired_fps = 120.0f;
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
}
