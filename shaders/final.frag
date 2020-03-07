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

layout(set = 1, binding = 0) uniform lighting_t {
    vec4 vs_light_positions[4];
    vec4 light_colors[4];
} u_lighting;

layout(set = 2, binding = 0) uniform camera_transforms_t {
    mat4 projection;
    mat4 view;
    mat4 view_projection;
    vec4 frustum;
    vec4 ws_camera_position;
} u_camera_transforms;

float linear_depth(
    float d,
    float z_near,
    float z_far) {
    return z_near * z_far / (z_far + d * (z_near - z_far));
}

const float PI = 3.14159265359f;

float distribution_ggx(
    float ndoth,
    float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float denom = ndoth * ndoth * (a2 - 1.0f) + 1.0f;
    denom = PI * denom * denom;

    return a2 / max(denom, 0.0001f);
}

float smith_ggx(
    float ndotv,
    float ndotl,
    float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    float ggx1 = ndotv / (ndotv * (1.0f - k) + k);
    float ggx2 = ndotl / (ndotl * (1.0f - k) + k);
    return ggx1 * ggx2;
}

vec3 fresnel(
    float hdotv,
    vec3 base) {
    return base + (1.0f - base) * pow(1.0f - clamp(hdotv, 0.0, 1.0), 5.0);
}

void main() {
    vec4 raw_albedo = texture(u_gbuffer_albedo, in_fs.uvs);
    vec4 raw_vs_normal = texture(u_gbuffer_normal, in_fs.uvs);
    vec3 vs_position = texture(u_gbuffer_position, in_fs.uvs).xyz;
    vec3 vs_view = vec3(0.0f, 0.0f, -1.0f);

    float roughness = raw_albedo.a;
    float metalness = raw_vs_normal.a;

    vec3 albedo = raw_albedo.rgb;
    vec3 vs_normal = raw_vs_normal.xyz;
    
    vec3 base_reflectivity = mix(vec3(0.04), albedo, metalness);
    vec3 l = vec3(0.0f);

    for (int i = 0; i < 4; ++i) {
        vec3 vs_light = normalize(u_lighting.vs_light_positions[i].xyz - vs_position);
        vec3 vs_halfway = normalize(vs_light + vs_view);

        float d = length(u_lighting.vs_light_positions[i].xyz - vs_position);
        float attenuation = 1.0 / (d * d);
        vec3 radiance = u_lighting.light_colors[i].rgb * attenuation;

        float ndotv = max(dot(vs_normal, vs_view), 0.00001f);
        float ndotl = max(dot(vs_normal, vs_light), 0.00001f);
        float hdotv = max(dot(vs_halfway, vs_view), 0.0f);
        float ndoth = max(dot(vs_normal, vs_halfway), 0.0f);

        float distribution_term = distribution_ggx(ndoth, roughness);
        float smith_term = smith_ggx(ndotv, ndotl, roughness);
        vec3 fresnel_term = fresnel(hdotv, base_reflectivity);

        vec3 specular = distribution_term * smith_term * fresnel_term;
        specular /= 4.0 * ndotv * ndotl;

        vec3 kd = vec3(1.0) - fresnel_term;

        kd *= 1.0f - metalness;

        l += (kd * albedo / PI + specular) * radiance * ndotl;
    }

    vec3 ambient = vec3(0.03) * albedo;
    vec3 color = ambient + l;

    color = color / (color + vec3(1.0f));
    color = pow(color, vec3(1.0f / 2.2f));

    out_final_color = vec4(color, 1.0f);
    
    //out_final_color = texture(u_gbuffer_albedo, in_fs.uvs);
}
