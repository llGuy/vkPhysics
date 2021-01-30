#include <stdint.h>
#include "cl_sound3d.hpp"

#include <al_load.hpp>
#include <al_sounddb.hpp>
#include <containers.hpp>

namespace cl {

static constexpr uint32_t MAX_SOURCES = 1000;

static stack_container_t<game_source_t> game_sources;

// The sources will be in the players' audio struct, or created on the fly (e.g. explosions)
static uint32_t sounds[S3DT_INVALID];

void init_game_sounds_3d() {
    auto hit = al::load_wav_file("assets/sound/hit.wav");
    sounds[S3DT_HIT] = al::register_sound(&hit);

    game_sources.init(MAX_SOURCES);
}

void spawn_sound(sound_3d_type_t type, const vector3_t &position) {
    uint32_t idx = game_sources.add();
    game_source_t *src =game_sources.get(idx);

    if (!src->is_initialised) {
        src->src = al::init_3d_source();
        src->is_initialised = 1;
    }
    src->src.update_position(position);
    src->src.update_velocity(vector3_t(0.0f));
    src->src.play_sound(al::get_sound(sounds[type]));

    LOG_INFO("Spawned sound\n");
}

void tick_sound3d() {
    for (uint32_t i = 0; i < game_sources.data_count; ++i) {
        auto *src = game_sources.get(i);
        if (!src->src.is_playing()) {
            src->is_initialised = 0;
            game_sources.remove(i);
        }
    }
}

}
