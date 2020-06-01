#include "ui.hpp"
#include <renderer/renderer.hpp>

static ui_box_t main_box;

void u_main_menu_init() {
    main_box.init(
        RT_CENTER,
        2.0f,
        ui_vector2_t(0.0f, 0.0f),
        ui_vector2_t(0.8f, 0.8f),
        NULL,
        0x46464646);
}

void u_submit_main_menu() {
    push_colored_ui_box(&main_box);
}
    
void u_main_menu_input() {

}
