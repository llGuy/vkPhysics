#version 450

layout(location = 0) in VS_DATA {
    vec2 uvs;
} in_fs;

layout(location = 0) out float out_final_ao;

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
    float width;
    float height;
} u_camera_transforms;

layout(binding = 0, set = 2) uniform sampler2D u_noise;

layout(binding = 0, set = 3) uniform kernel_t {
    vec4 kernels[64];
    float resolution_coefficient;
} u_kernels;

const float div_magic = 1.0f / (4.0f * 0.0000001f);
const float div_total = 1.0f / 50.0f;
const float div_fade_distance_cap = 1.0f / 100.0f;

void main() {
    vec2 noise_scale = vec2(u_camera_transforms.width, u_camera_transforms.height) * div_magic;

    vec3 vs_normal = texture(u_gbuffer_normal, in_fs.uvs).xyz;

    if (vs_normal.x > -10.0f) {
        vec3 vs_position = texture(u_gbuffer_position, in_fs.uvs).xyz;
        vec3 random = texture(u_noise, in_fs.uvs * noise_scale).xyz;

        vec3 tangent = normalize(random - vs_normal * dot(random, vs_normal));
        vec3 bitangent = cross(vs_normal, tangent);
        mat3 tangent_space = mat3(tangent, bitangent, vs_normal);

        float occlusion = 0.0f;

        // Depends on pixel distance to camera
        float pixel_distance = abs(vs_position.z);
        float kernel_size = mix(0.6f, 3.0f, pixel_distance * div_fade_distance_cap);
        float bias = mix(0.02f, 0.2f, pixel_distance * div_fade_distance_cap);
        
        for (int i = 0; i < 50; ++i) {
            vec3 sample_kernel = tangent_space * vec3(u_kernels.kernels[i]);

            sample_kernel = vs_position + sample_kernel * kernel_size;

            vec4 offset = vec4(sample_kernel, 1.0f);
            offset = u_camera_transforms.projection * offset;
            offset.xyz /= offset.w;
            offset.xyz = offset.xyz * 0.5f + 0.5f;

            float sample_depth = texture(u_gbuffer_position, offset.xy).z;
            
            float range = smoothstep(0.25f, 1.0f, 0.5f / abs(vs_position.z - sample_depth));
            occlusion += (sample_depth >= sample_kernel.z + bias ? 1.0f : 0.0f) * range;
        }

        occlusion = 1.0f - (occlusion * div_total);
        out_final_ao = occlusion;
    }
    else {
        out_final_ao = 1.0f;
    }
}
