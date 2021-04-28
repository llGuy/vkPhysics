#version 450

layout(location = 0) in VS_DATA {
    vec3 local_position;
} in_fs;

layout(location = 0) out vec4 final_color;

layout(set = 0, binding = 0) uniform samplerCube u_environment;

layout(push_constant) uniform push_constant_t {
    mat4 matrix;
    float roughness;

    float layer;
} u_push_constant;

const float PI = 3.14159265f;

float distribution_ggx(vec3 n, vec3 h, float roughness){
    float a = roughness * roughness;
    float a2 = a * a;
    float ndoth = max(dot(n, h), 0.0);
    float ndoth2 = ndoth * ndoth;

    float nom   = a2;
    float denom = (ndoth2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}

vec3 importance_sample_ggx(vec2 xi, vec3 n, float roughness) {
    float a = roughness * roughness;
	
    float phi = 2.0 * PI * xi.x;
    float cos_theta = sqrt((1.0 - xi.y) / (1.0 + (a * a - 1.0) * xi.y));
    float sin_theta = sqrt(1.0 - cos_theta * cos_theta);
	
    vec3 h;
    h.x = cos(phi) * sin_theta;
    h.y = sin(phi) * sin_theta;
    h.z = cos_theta;
	
    vec3 up        = abs(n.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent   = normalize(cross(up, n));
    vec3 bitangent = cross(n, tangent);
	
    vec3 sample_vec = tangent * h.x + bitangent * h.y + n * h.z;
    return normalize(sample_vec);
}

float radical_inverse_vandercorpus(uint bits)  {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}

vec2 hammersley(uint i, uint n) {
    return vec2(float(i) / float(n), radical_inverse_vandercorpus(i));
}  

void main() {
    vec3 n = normalize(in_fs.local_position);
    vec3 r = n;
    vec3 v = r;

    const uint SAMPLE_COUNT = 1024u;
    float total_weight = 0.0;
    vec3 prefiltered_color = vec3(0.0);
    
    for(uint i = 0u; i < SAMPLE_COUNT; ++i) {
        vec2 xi = hammersley(i, SAMPLE_COUNT);
        vec3 h  = importance_sample_ggx(xi, n, u_push_constant.roughness);
        vec3 l  = normalize(2.0 * dot(v, h) * h - v);

        float ndotl = max(dot(n, l), 0.0);
        if(ndotl > 0.0) {
            prefiltered_color += texture(u_environment, l).rgb * ndotl;
            total_weight += ndotl;
        }
    }
    
    prefiltered_color = prefiltered_color / total_weight;

    final_color = vec4(prefiltered_color, 1.0);
}
