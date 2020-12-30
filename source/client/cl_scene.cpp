#include "cl_scene.hpp"
#include <ux_scene.hpp>
#include "allocators.hpp"

namespace cl {

static ux::scene_t *main_scene;
static ux::scene_t *play_scene;
static ux::scene_t *map_creator_scene;

void prepare_scenes(vkph::state_t *state) {
    main_scene = flmalloc<main_scene_t>();
    new(main_scene) main_scene_t();

    play_scene = flmalloc<play_scene_t>();
    new(play_scene) play_scene_t();

    map_creator_scene = flmalloc<map_creator_scene_t>();
    new(map_creator_scene) map_creator_scene_t();

    ux::push_scene(main_scene);
    ux::push_scene(play_scene);
    ux::push_scene(map_creator_scene);

    ux::init_scenes(state);
}

}
