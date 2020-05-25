#include "ui.hpp"
#include <renderer/renderer.hpp>

// HUD for now, just contains a crosshair
static ui_box_t crosshair;

void u_hud_init() {
    crosshair.init(
        RT_CENTER,
        1.0f,
        ui_vector2_t(-0.02f, -0.02f),
        ui_vector2_t(0.04f, 0.04f),
        NULL,
        0xFFFFFFFF);
}

void u_submit_hud() {
    push_colored_ui_box(&crosshair);
}
