#version 450

layout(location = 0) in GS_DATA {
    vec3 normal;
    vec3 cube_face_direction;
    mat4 rotation;
} in_fs;

layout(location = 0) out vec4 out_final_color;

layout(push_constant) uniform push_constant_t {
    mat4 inverse_projection;
    float width;
    float height;

    float light_direction_x;
    float light_direction_y;
    float light_direction_z;
    float eye_height;
    float rayleigh;
    float mie;
    float intensity;
    float scatter_strength;
    float rayleigh_strength;
    float mie_strength;
    float rayleigh_collection;
    float mie_collection;
    float air_color_r;
    float air_color_g;
    float air_color_b;
} u_push_constant;

mat4 look_at(
    vec3 eye,
    vec3 center,
    vec3 up)
{
    vec3 f = vec3(normalize(center - eye));
    vec3 s = vec3(normalize(cross(f, up)));
    vec3 u = vec3(cross(s, f));

    mat4 m = mat4(1.0f);
    m[0][0] = s.x;
    m[1][0] = s.y;
    m[2][0] = s.z;
    m[0][1] = u.x;
    m[1][1] = u.y;
    m[2][1] = u.z;
    m[0][2] =-f.x;
    m[1][2] =-f.y;
    m[2][2] =-f.z;
    m[3][0] =-dot(s, eye);
    m[3][1] =-dot(u, eye);
    m[3][2] = dot(f, eye);
    return m;
}

float atmospheric_depth(vec3 position, vec3 dir){
    float a = dot(dir, dir);
    float b = 2.0f * dot(dir, position);
    float c = dot(position, position) - 1.0f;
    float det = b * b - 4.0f * a * c;
    float det_sqrt = sqrt(det);
    float q = (-b - det_sqrt) / 2.0f;
    float t1 = c / q;
    return t1;
}

const float EYE_HEIGHT = 0.0f;
const int NUM_SAMPLES_I = 20;
const float NUM_SAMPLES = 20.0f;

vec3 compute_spherical_view_direction() {
    //mat3 inverse_view_rotate = inverse(mat3(look_at(vec3(0.0f), in_fs.cube_face_direction, vec3(0.0f, 1.0f, 0.0f))));
    vec2 ds_direction = gl_FragCoord.xy / vec2(u_push_constant.width, u_push_constant.height);

    /*if (gl_Layer == 2 || gl_Layer == 3) {
        ds_direction.y = 1.0f - ds_direction.y;
    }*/

    //ds_direction.y = 1.0f - ds_direction.y;
    ds_direction -= vec2(0.5f);
    ds_direction *= 2.0f;

    vec3 direction = normalize((u_push_constant.inverse_projection * vec4(ds_direction, 0.0f, 1.0f)).xyz);

    return normalize(mat3(in_fs.rotation) * direction);
}

float horizon_extinction(vec3 position, vec3 dir, float radius) {
    float u = dot(dir, -position);
    if(u < 0.0){
        return 1.0;
    }
    vec3 near = position + u*dir;
    if(length(near) < radius){
        return 0.0;
    }
    else{
        vec3 v2 = normalize(near)*radius - position;
        float diff = acos(dot(normalize(v2), dir));
        return smoothstep(0.0, 1.0, pow(diff*2.0, 3.0));
    }
}

const vec3 LIGHT_DIR = normalize(vec3(0.0f, 0.0f, 0.1f));

float phase(float alpha, float g){
    float a = 3.0f * (1.0f - g* g);
    float b = 2.0f * (2.0f + g * g);
    float c = 1.0f + alpha * alpha;
    float d = pow(1.0f + g * g - 2.0f * g * alpha, 1.5f);
    return (a / b) * (c / d);
}

//vec3 kr = vec3(
//    0.18867780436772762, 0.4978442963618773, 0.6616065586417131
//);

vec3 absorb(float d, vec3 color, float factor, vec3 kr) {
    return color - color * pow(kr, vec3(factor/  d));
}

void main() {
    vec3 kr = vec3(u_push_constant.air_color_r, u_push_constant.air_color_g, u_push_constant.air_color_b);
    
    vec3 eye_direction = compute_spherical_view_direction();
    vec3 eye_position = vec3(0.0f, u_push_constant.eye_height, 0.0f);

    vec3 light_dir = normalize(vec3(u_push_constant.light_direction_x, u_push_constant.light_direction_y, u_push_constant.light_direction_z));
    light_dir.xz *= -1.0f;
    
    float alpha = dot(eye_direction, light_dir);
    
    float rayleigh = phase(alpha, u_push_constant.rayleigh);
    float mie = phase(alpha, u_push_constant.mie);
    
    float ray_length = atmospheric_depth(eye_position, eye_direction);
    float ray_step_length = ray_length / NUM_SAMPLES;
    vec3 ray_step = eye_direction * ray_step_length;
    vec3 ray_sample_point = eye_position;

    vec3 accumulated_rayleigh = vec3(0.0f);
    vec3 accumulated_mie = vec3(0.0f);

    float eye_extinction = horizon_extinction(eye_position, eye_direction, u_push_constant.eye_height - 0.25f);
    
    float intensity = u_push_constant.intensity;

    float spot = smoothstep(0.0, 15.0, phase(alpha, 0.9995)) * 5.0f;
    
    for (int i = 0; i < NUM_SAMPLES_I; ++i) {
        float sample_distance = ray_step_length * float(i);
        vec3 ray_sample_position = eye_position + sample_distance * eye_direction;

        float extinction = horizon_extinction(ray_sample_position, light_dir, u_push_constant.eye_height - 0.35f);
        
        float sample_depth = atmospheric_depth(eye_position, light_dir);

        vec3 influx = absorb(sample_depth, vec3(intensity), u_push_constant.scatter_strength, kr) * extinction;

        accumulated_rayleigh += absorb(sample_distance, kr * influx, u_push_constant.rayleigh_strength, kr);
        accumulated_mie += absorb(sample_distance, influx, u_push_constant.mie_strength, kr);
    }

    accumulated_rayleigh = (accumulated_rayleigh * pow(ray_length, u_push_constant.rayleigh_strength)) / NUM_SAMPLES;
    accumulated_mie = (accumulated_mie * pow(ray_length, u_push_constant.mie_strength)) / NUM_SAMPLES;

    vec3 color = vec3(mie * accumulated_mie + rayleigh * accumulated_rayleigh);

    float a = 0.0f;
    if (dot(-light_dir, eye_direction) > 0.985) {
        a = 1.0f;
    }
    
    out_final_color = vec4(color, a);
}
