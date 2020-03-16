#version 450

layout(location = 0) in VS_DATA {
    vec2 uvs;
} in_fs;

layout(location = 0) out vec4 out_final_color;
layout(location = 1) out vec4 out_bright_color;

layout(binding = 0, set = 0) uniform sampler2D u_gbuffer_albedo;
layout(binding = 1, set = 0) uniform sampler2D u_gbuffer_normal;
layout(binding = 2, set = 0) uniform sampler2D u_gbuffer_position;
// TODO: Get position from depth buffer - for now, use gbuffer position as testcase
layout(binding = 3, set = 0) uniform sampler2D u_gbuffer_depth;

layout(binding = 0, set = 3) uniform samplerCube u_irradiance_map;

layout(binding = 0, set = 4) uniform sampler2D u_integral_lookup;
layout(binding = 0, set = 5) uniform samplerCube u_prefilter_map;

layout(binding = 0, set = 6) uniform sampler2D u_ao;

layout(set = 1, binding = 0) uniform lighting_t {
    vec4 vs_light_positions[4];
    vec4 light_colors[4];
    vec4 vs_directional_light;
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

void main() {
    vec4 raw_albedo = texture(u_gbuffer_albedo, in_fs.uvs);
    vec4 raw_vs_normal = texture(u_gbuffer_normal, in_fs.uvs);
    vec3 vs_position = texture(u_gbuffer_position, in_fs.uvs).xyz;
    vec3 vs_view = normalize(-vs_position);
    
    vec3 color = raw_albedo.rgb;

    vec2 diff = 1.0f / vec2(u_camera_transforms.width, u_camera_transforms.height);
    
    if (raw_vs_normal.x > -10.0f) {
        // Detect edge
        /*vec3 total = vec3(0.0f);
        float count = 0.0f;
        
        if (raw_vs_normal.x > -10.0f) {

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

        raw_vs_normal.xyz = total;*/

        vec3 vs_normal = normalize(raw_vs_normal.xyz);
        
        float roughness = raw_albedo.a;
        float metalness = raw_vs_normal.a;

        vec3 albedo = raw_albedo.rgb;
        vec3 base_reflectivity = mix(vec3(0.04), albedo, metalness);
    
        vec3 l = vec3(0.0f);

        //for (int i = 0; i < 4; ++i) {
            //vec3 light_position = vec3(u_lighting.vs_light_positions[i]);
        
            //vec3 vs_light = normalize(light_position - vs_position);
            vec3 vs_light = -normalize(u_lighting.vs_directional_light.xyz);
            vec3 vs_halfway = normalize(vs_light + vs_view);

            //float d = length(light_position - vs_position);
            //float attenuation = 1.0 / (d * d);
            float attenuation = 1.0f;

            vec3 ws_light = vec3(u_camera_transforms.inverse_view * vec4(vs_light, 0.0f));
            ws_light.y *= -1.0f;
            vec3 directional_light_color =  textureLod(u_prefilter_map, ws_light, 0.0f).rgb;
            
            //vec3 radiance = u_lighting.light_colors[i].rgb * attenuation;
            vec3 radiance = directional_light_color * attenuation;

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

            l += (kd * albedo / PI + specular) * radiance * ndotl;
            //l += (kd * albedo / PI + specular) * ndotl;
        //}

        vec3 fresnel = fresnel_roughness(max(dot(vs_view, vs_normal), 0.000001f), base_reflectivity, roughness);
        //vec3 kd = (vec3(1.0f) - fresnel) * (1.0f - metalness);
        kd = (vec3(1.0f) - fresnel) * (1.0f - metalness);

        vec3 ws_normal = vec3(u_camera_transforms.inverse_view * vec4(vs_normal, 0.0f));
        vec3 ws_view = vec3(u_camera_transforms.inverse_view * vec4(vs_view, 0.0f));
        
        vec3 diffuse = texture(u_irradiance_map, vec3(ws_normal.x, -ws_normal.y, ws_normal.z)).xyz * albedo * kd;

        const float MAX_REFLECTION_LOD = 4.0f;
        vec3 reflected_vector = reflect(-ws_view, ws_normal);
        reflected_vector.y *= -1.0f;
        vec3 prefiltered_color = textureLod(u_prefilter_map, reflected_vector, roughness * MAX_REFLECTION_LOD).rgb;
        vec2 brdf = texture(u_integral_lookup, vec2(max(dot(ws_normal, ws_view), 0.0f), roughness)).rg;
        //vec3 specular = prefiltered_color * (fresnel * brdf.r + clamp(brdf.g, 0, 1));
        specular = prefiltered_color * (fresnel * brdf.r + clamp(brdf.g, 0, 1));
        //vec3 specular = vec3(1.0f) * (fresnel * brdf.r);

        float ao = texture(u_ao, in_fs.uvs).r;
        
        vec3 ambient = (diffuse + specular);
        //vec3 ambient = vec3(ao);
        
        //vec3 ambient = vec3(0.03) * albedo;
        
        color = (ambient + l) * pow(ao, 8.0f);
        //color = vec3(ao);

        //color = color / (color + vec3(1.0f));
        //color = pow(color, vec3(1.0f / 2.2f));
    }

    float brightness = dot(color, vec3(0.2126f, 0.7152f, 0.0722f));
    if (brightness > 1.0f) {
        out_bright_color = vec4(color, 1.0f);
    }
    else {
        out_bright_color = vec4(0.0f, 0.0f, 0.0f, 1.0f);
    }

    out_final_color = vec4(color, 1.0f);

//    out_final_color = vec4(texture(u_ao, in_fs.uvs).r);
}
