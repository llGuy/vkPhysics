#include "vk_memory.hpp"
#include "vk_context.hpp"
#include "vk_scene3d_environment.hpp"

namespace vk {

void atmosphere_model_default_values(atmosphere_model_t *model, const vector3_t &directional_light) {
    float eye_height = 0.0f;
    float light_direction[3] = { directional_light.x, directional_light.y, directional_light.z };
    float rayleigh = -0.082f;
    float mie = -0.908f;
    float intensity = 0.650f;
    float scatter_strength = 1.975f;
    float rayleigh_strength = 2.496f;
    float mie_strength = 0.034f;
    float rayleigh_collection = 8.0f;
    float mie_collection = 2.981f;
    float kr[3] = {27.0f / 255.0f, 82.0f / 255.0f, 111.0f / 255.0f};
    
    model->eye_height = eye_height;
    model->light_direction_x = light_direction[0];
    model->light_direction_y = light_direction[1];
    model->light_direction_z = light_direction[2];
    model->rayleigh = rayleigh;
    model->mie = mie;
    model->intensity = intensity;
    model->scatter_strength = scatter_strength;
    model->rayleigh_strength = rayleigh_strength;
    model->mie_strength = mie_strength;
    model->rayleigh_collection = rayleigh_collection;
    model->mie_collection = mie_collection;
    model->air_color_r = kr[0];
    model->air_color_g = kr[1];
    model->air_color_b = kr[2];
}

}
