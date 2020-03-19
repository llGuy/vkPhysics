#version 450

layout(location = 0) in VS_DATA {
    vec2 uvs;
} in_fs;

layout(location = 0) out vec4 out_final_color;
layout(location = 1) out vec4 out_bright_color;

layout(binding = 0, set = 0) uniform sampler2D u_gbuffer_albedo;
layout(binding = 1, set = 0) uniform sampler2D u_gbuffer_normal;
layout(binding = 2, set = 0) uniform sampler2D u_gbuffer_position;
layout(binding = 3, set = 0) uniform sampler2D u_gbuffer_sun;
// TODO: Get position from depth buffer - for now, use gbuffer position as testcase
layout(binding = 4, set = 0) uniform sampler2D u_gbuffer_depth;

layout(binding = 0, set = 3) uniform samplerCube u_irradiance_map;

layout(binding = 0, set = 4) uniform sampler2D u_integral_lookup;
layout(binding = 0, set = 5) uniform samplerCube u_prefilter_map;

layout(binding = 0, set = 6) uniform sampler2D u_ao;
layout(binding = 0, set = 7) uniform sampler2D u_shadow_map;

layout(set = 1, binding = 0) uniform lighting_t {
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

layout(set = 2, binding = 0) uniform camera_transforms_t {
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

    return a2 / max(denom, 0.000001f);
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
    return base + (1.0f - base) * pow(1.0f - clamp(hdotv, 0.0f, 1.0f), 5.0f);
}

vec3 fresnel_roughness(
    float ndotv,
    vec3 base,
    float roughness) {
    return base + (max(vec3(1.0f - roughness), base) - base) * pow(1.0f - ndotv, 5.0f);
}

// Not being used for the moment
bool get_shadow_factor(
    vec3 ws_position,
    out float occlusion) {
    float pcf_count = 2.0f;
    
    vec4 ls_position = u_lighting.shadow_view_projection * vec4(ws_position, 1.0f);

    ls_position.xyz /= ls_position.w;
    ls_position.xy = ls_position.xy * 0.5f + 0.5f;

    vec2 texel_size = 1.0f / (textureSize(u_shadow_map, 0));
    //float depth = texture(u_shadow_map, ls_position.xy).r;

    occlusion = 0.0f;

    bool occluded = false;
    
    for (int x = int(-pcf_count); x <= int(pcf_count); ++x) {
        for (int y = int(-pcf_count); y <= int(pcf_count); ++y) {
            float depth = texture(u_shadow_map, ls_position.xy).r;
            
            if (ls_position.z - 0.0009f > depth) {
                occlusion += 0.95f;
                occluded = true;
            }
        }
    }

    occlusion /= (pcf_count * 2.0f + 1.0f) * (pcf_count * 2.0f + 1.0f);
    occlusion = 1.0f - occlusion;

    return occluded;
}

vec3 round_edge(
    in vec4 raw_vs_normal,
    in vec4 vs_position) {
    vec3 total = vec3(0.0f);
    float count = 0.0f;
    
    if (raw_vs_normal.x > -10.0f) {
        vec2 diff = 1.0f / vec2(u_camera_transforms.width, u_camera_transforms.height);

        for (int y = -3; y < 3; ++y) {
            for (int x = -3; x < 3; ++x) {
                vec2 coord = vec2( float(x), float(y) ) * diff * 1.01f + in_fs.uvs;
                float depth = texture(u_gbuffer_position, coord).z;
                if (abs(depth - vs_position.z) < 0.8f) {
                    // Calculate normal
                    float coeff = abs(float(x) * float(y));
                    vec4 n = texture(u_gbuffer_normal, coord);
                    if (n.x > -10.0f) {
                        total += n.xyz * coeff;

                        count += coeff;
                    }
                }
            }
        }
    }

    total /= count;

    return total;
}

bool should_compute_lighting(
    in vec4 raw_vs_normal) {
    return raw_vs_normal.x > -10.0f;
}

vec3 directional_luminance(
    in vec3 albedo,
    in vec3 vs_normal,
    in vec3 vs_view,
    in vec3 ws_position,
    in vec3 base_reflectivity,
    in float roughness,
    in float metalness) {
    vec3 vs_light = -normalize(u_lighting.vs_directional_light.xyz);
    vec3 vs_halfway = normalize(vs_light + vs_view);

    vec3 ws_light = -u_lighting.ws_directional_light.xyz;
    ws_light.y *= -1.0f;

    vec3 directional_light_color =  textureLod(u_prefilter_map, ws_light, 0.0f).rgb;
    vec3 opposite_light_color = textureLod(u_prefilter_map, -ws_light, 0.0f).rgb;
    float intensity = abs(dot(opposite_light_color, opposite_light_color));

    vec3 radiance = directional_light_color;

    float ndotv = max(dot(vs_normal, vs_view), 0.000001f);
    float ndotl = max(dot(vs_normal, vs_light), 0.000001f);
    float hdotv = max(dot(vs_halfway, vs_view), 0.000001f);
    float ndoth = max(dot(vs_normal, vs_halfway), 0.000001f);

    float distribution_term = distribution_ggx(ndoth, roughness);
    float smith_term = smith_ggx(ndotv, ndotl, roughness);
    vec3 fresnel_term = fresnel(hdotv, base_reflectivity);

    vec3 specular = smith_term * distribution_term * fresnel_term;
    specular /= 4.0 * ndotv * ndotl;

    vec3 kd = vec3(1.0) - fresnel_term;

    kd *= 1.0f - metalness;

    return (kd * albedo / PI + specular) * radiance * ndotl;
}

vec3 point_luminance(
    int light_index,
    in vec3 albedo,
    in vec3 vs_position,
    in vec3 vs_normal,
    in vec3 vs_view,
    in vec3 ws_position,
    in vec3 base_reflectivity,
    in float roughness,
    in float metalness) {
    vec3 light_position = vec3(u_lighting.vs_light_positions[light_index]);
    vec3 vs_view_light_direction = normalize(vec3(u_lighting.vs_light_directions[light_index]).xyz);
    vec3 vs_light = normalize(light_position - vs_position);

    float cutoff = 0.95f;
    float outer_cutoff = 0.8f;
    
    float theta = dot(vs_light, -vs_view_light_direction);
    float eps = cutoff - outer_cutoff;
    float intensity = clamp((theta - outer_cutoff) / eps, 0.0f, 1.0f);

    vec3 vs_halfway = normalize(vs_light + vs_view);

    // TODO: Replace with dot product
    float d = length(light_position - vs_position);
    float attenuation = 1.0 / (d * d);

    vec3 radiance = u_lighting.light_colors[light_index].rgb * attenuation;
    
    float ndotv = max(dot(vs_normal, vs_view), 0.000001f);
    float ndotl = max(dot(vs_normal, vs_light), 0.000001f);
    float hdotv = max(dot(vs_halfway, vs_view), 0.000001f);
    float ndoth = max(dot(vs_normal, vs_halfway), 0.000001f);

    float distribution_term = distribution_ggx(ndoth, roughness);
    float smith_term = smith_ggx(ndotv, ndotl, roughness);
    vec3 fresnel_term = fresnel(hdotv, base_reflectivity);

    vec3 specular = smith_term * distribution_term * fresnel_term;
    specular /= 4.0 * ndotv * ndotl;

    vec3 kd = vec3(1.0) - fresnel_term;
    kd *= 1.0f - metalness;

    return (kd * albedo / PI + specular) * radiance * ndotl * intensity;
}

vec4 bright_color(
    vec3 color) {
    float brightness = dot(color, vec3(0.2126f, 0.7152f, 0.0722f));
    if (brightness > 10.0f) {
        return vec4(color, 1.0f);
    }
    else {
        return vec4(0.0f, 0.0f, 0.0f, 1.0f);
    }
}

void main() {
    vec4 raw_albedo = texture(u_gbuffer_albedo, in_fs.uvs);
    vec4 raw_vs_normal = texture(u_gbuffer_normal, in_fs.uvs);
    vec3 vs_position = texture(u_gbuffer_position, in_fs.uvs).xyz;
    vec3 vs_view = normalize(-vs_position);
    
    vec3 color = raw_albedo.rgb;
    
    if (should_compute_lighting(raw_vs_normal)) {
        vec3 vs_normal = raw_vs_normal.xyz;
        
        // World space calculations
        vec3 ws_position = vec3(u_camera_transforms.inverse_view * vec4(vs_position, 1.0f));
        vec3 ws_normal = vec3(u_camera_transforms.inverse_view * vec4(vs_normal, 0.0f));
        vec3 ws_view = vec3(u_camera_transforms.inverse_view * vec4(vs_view, 0.0f));
        
        float roughness = raw_albedo.a;
        float metalness = raw_vs_normal.a;

        vec3 albedo = raw_albedo.rgb;
        vec3 base_reflectivity = mix(vec3(0.04), albedo, metalness);
    
        vec3 l = vec3(0.0f);

        l += directional_luminance(
            albedo,
            vs_normal,
            vs_view,
            ws_position,
            base_reflectivity,
            roughness,
            metalness);

        for (int i = 0; i < u_lighting.point_light_count; ++i) {
            l += point_luminance(
                i,
                albedo,
                vs_position,
                vs_normal,
                vs_view,
                ws_position,
                base_reflectivity,
                roughness,
                metalness);
        }

        vec3 fresnel = fresnel_roughness(max(dot(vs_view, vs_normal), 0.000001f), base_reflectivity, roughness);
        vec3 kd = (vec3(1.0f) - fresnel) * (1.0f - metalness);
        
        vec3 diffuse = texture(u_irradiance_map, vec3(ws_normal.x, -ws_normal.y, ws_normal.z)).xyz * albedo * kd;

        const float MAX_REFLECTION_LOD = 4.0f;
        vec3 reflected_vector = reflect(-ws_view, ws_normal);
        reflected_vector.y *= -1.0f;
        vec3 prefiltered_color = textureLod(u_prefilter_map, reflected_vector, roughness * MAX_REFLECTION_LOD).rgb;
        vec2 brdf = texture(u_integral_lookup, vec2(max(dot(ws_normal, ws_view), 0.0f), roughness)).rg;
        vec3 specular = prefiltered_color * (fresnel * brdf.r + clamp(brdf.g, 0, 1));

        float ao = texture(u_ao, in_fs.uvs).r;
        
        vec3 ambient = (diffuse + specular);
        
        color = (ambient + l) * pow(ao, 10.0f);
    }

    out_final_color = vec4(color, 1.0f);
    // Not enabling bloom for the moment
    out_bright_color = vec4(0.0f);
}
