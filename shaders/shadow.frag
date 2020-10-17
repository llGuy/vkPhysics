#version 450

layout(location = 0) out vec2 out_moment;

layout(location = 0) in VS_DATA {
    float depth;
} in_fs;

void main() {
    float depth = gl_FragCoord.z;

    float dx = dFdx(depth);
    float dy = dFdy(depth);
    float sigma = depth * depth + 0.25 * (dx * dx + dy * dy);

    out_moment = vec2(depth, sigma);
}
