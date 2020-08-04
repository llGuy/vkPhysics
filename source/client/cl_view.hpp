#pragma once

// View describes not only some aspects of how to render the scene
// But also dictates how the user will be sending input
enum game_view_type_t {
    GVT_MENU,
    GVT_IN_GAME,
    GVT_INVALID
};

void cl_change_view_type(game_view_type_t view_type);
game_view_type_t cl_get_current_game_view_type();
