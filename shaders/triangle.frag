#version 450

layout(location = 0) out vec4 out_albedo;
layout(location = 1) out vec4 out_normal;
layout(location = 2) out vec4 out_extra;

void main() {
     out_albedo = vec4(1.0f, 0.0f, 0.0f, 1.0f);
     out_normal = vec4(0.0f,0.0f, 0.0f, 1.0f);
     out_extra = vec4(0.0f, 0.0f, 0.0f, 1.0f);
}
