#include "cl_view.hpp"
#include "cl_main.hpp"
#include "cl_frame.hpp"
#include <log.hpp>
#include <app.hpp>

namespace cl {

static game_view_type_t current_view_type;

void cl_change_view_type(game_view_type_t view) {
    current_view_type = view;

    switch (view) {
        
        // In GV_MAIN_MENU, *a* world gets rendered, however it isn't a typically loaded world
        // it's just a world that was loaded from disk (just the vertices). The rendering is
        // also blurred for cool main menu kind of effect.
    case GVT_MENU: {
        app::enable_cursor();
        get_frame_info()->blurred = 1;
        get_frame_info()->ssao = 0;
    } break;

    case GVT_IN_GAME: {
        app::disable_cursor();
        get_frame_info()->blurred = 0;
        get_frame_info()->ssao = 1;
    } break;

    case GVT_INVALID: {
        LOG_ERROR("Changing view to invalid view\n");
    } break;

    }
}

game_view_type_t cl_get_current_game_view_type() {
    return current_view_type;
}

}
