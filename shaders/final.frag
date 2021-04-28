#version 450

layout(location = 0) in VS_DATA {
    vec2 uvs;
} in_fs;

layout(location = 0) out vec4 out_final_color;

layout(binding = 0, set = 0) uniform sampler2D u_diffuse;

layout(push_constant) uniform push_constant_t {
    float alpha; // For fading and stuff
} u_push_constant;

void main() {
    out_final_color = texture(u_diffuse, in_fs.uvs);// + texture(u_bloom, in_fs.uvs);
    
    vec3 color = out_final_color.rgb;

    color = color / (color + vec3(1.0f));
    color = pow(color, vec3(1.0f / 2.2f));

    out_final_color = vec4(color, 1.0f) * u_push_constant.alpha;
}
