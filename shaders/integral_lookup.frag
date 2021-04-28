#version 450

layout (location = 0) in VS_DATA {
    vec2 uvs;
} in_fs;

layout(location = 0) out vec2 out_final_color;

const float PI = 3.14159265359f;

float geometry_ggx(float ndotv, float roughness) {
    float a = roughness;
    float k = (a * a) / 2.0;

    float nom = ndotv;
    float denom = ndotv * (1.0 - k) + k;

    return nom / denom;
}

float geometry_smith(vec3 n, vec3 v, vec3 l, float roughness) {
    float ndotv = max(dot(n, v), 0.0);
    float ndotl = max(dot(n, l), 0.0);
    float ggx2 = geometry_ggx(ndotv, roughness);
    float ggx1 = geometry_ggx(ndotl, roughness);

    return ggx1 * ggx2;
}

vec3 importance_sample(vec2 xi, vec3 n, float roughness) {
    float a = roughness * roughness;
	
    float phi = 2.0 * PI * xi.x;
    float cos_theta = sqrt((1.0 - xi.y) / (1.0 + (a * a - 1.0) * xi.y));
    float sin_theta = sqrt(1.0 - cos_theta * cos_theta);
	
    vec3 h;
    h.x = cos(phi) * sin_theta;
    h.y = sin(phi) * sin_theta;
    h.z = cos_theta;
	
    // from tangent-space vector to world-space sample vector
    vec3 up = abs(n.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent   = normalize(cross(up, n));
    vec3 bitangent = cross(n, tangent);
	
    vec3 sample_vec = tangent * h.x + bitangent * h.y + n * h.z;
    return normalize(sample_vec);
}

float radical_inverse(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

vec2 hammersley(uint i, uint n) {
    return vec2(float(i) / float(n), radical_inverse(i));
}  

vec2 integrate(float ndotv, float roughness) {
    vec3 v;
    v.x = sqrt(1.0 - ndotv * ndotv);
    v.y = 0.0;
    v.z = ndotv;

    float a = 0.0;
    float b = 0.0;

    vec3 n = vec3(0.0, 0.0, 1.0);

    const uint SAMPLE_COUNT = 1024u;
    
    for(uint i = 0u; i < SAMPLE_COUNT; ++i) {
        vec2 xi = hammersley(i, SAMPLE_COUNT);
        vec3 h  = importance_sample(xi, n, roughness);
        vec3 l  = normalize(2.0 * dot(v, h) * h - v);

        float ndotl = max(l.z, 0.0);
        float ndoth = max(h.z, 0.0);
        float vdoth = max(dot(v, h), 0.0);

        if(ndotl > 0.0) {
            float g = geometry_smith(n, v, l, roughness);
            float g_vis = (g * vdoth) / (ndoth * ndotv);
            float fc = pow(1.0 - vdoth, 5.0);

            a += (1.0 - fc) * g_vis;
            b += fc * g_vis;
        }
    }
    a /= float(SAMPLE_COUNT);
    b /= float(SAMPLE_COUNT);
    return vec2(a, b);
}

void main()  {
    vec2 integrated = integrate(in_fs.uvs.x, in_fs.uvs.y);
    out_final_color = integrated;
}
