#include "game/u_internal.hpp"
#include "ui.hpp"
#include <renderer/renderer.hpp>

// HUD for now, just contains a crosshair
static ui_box_t crosshair;
static VkDescriptorSet crosshair_texture;

static struct {
    uint32_t current_crosshair;
    vector2_t uvs[6];
} crosshair_selection;

void u_hud_init() {
    crosshair.init(
        RT_CENTER,
        1.0f,
        ui_vector2_t(0.0f, 0.0f),
        ui_vector2_t(0.05f, 0.05f),
        NULL,
        0xFFFFFFFF);

    crosshair_texture = u_texture(UT_CROSSHAIRS);

    crosshair_selection.current_crosshair = 1;
}

void u_submit_hud() {
    float unit = (1.0f / 8.0f);

    vector2_t start = vector2_t(crosshair_selection.current_crosshair % 8, crosshair_selection.current_crosshair / 8) / 8.0f;
    crosshair_selection.uvs[0] = vector2_t(start.x, start.y + unit);
    crosshair_selection.uvs[1] = crosshair_selection.uvs[3] = vector2_t(start.x, start.y);
    crosshair_selection.uvs[2] = crosshair_selection.uvs[4] = vector2_t(start.x + unit, start.y + unit);
    crosshair_selection.uvs[5] = vector2_t(start.x + unit, start.y);

    mark_ui_textured_section(crosshair_texture);

    push_textured_ui_box(&crosshair, crosshair_selection.uvs);
}
