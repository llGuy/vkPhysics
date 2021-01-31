#include "cl_scene.hpp"
#include <ux_scene.hpp>
#include "allocators.hpp"

namespace cl {

static ux::scene_t *main_scene;
static ux::scene_t *play_scene;
static ux::scene_t *map_creator_scene;
static ux::scene_t *debug_scene;

void prepare_scenes(vkph::state_t *state) {
    main_scene = flmalloc_and_init<main_scene_t>();
    play_scene = flmalloc_and_init<play_scene_t>();
    map_creator_scene = flmalloc_and_init<map_creator_scene_t>();
    debug_scene = flmalloc_and_init<debug_scene_t>();

    ux::push_scene(main_scene);
    ux::push_scene(play_scene);
    ux::push_scene(map_creator_scene);
    ux::push_scene(debug_scene);

    ux::init_scenes(state);
}

}
