#pragma once

#include <vkph_state.hpp>
#include "cl_frame.hpp"
#include "cl_render.hpp"
#include <vkph_event.hpp>
#include <vkph_state.hpp>
#include <ux_scene.hpp>

namespace cl {

void prepare_scenes(vkph::state_t *state);

enum scene_type_t {
    ST_MAIN = 0,
    ST_PLAY = 1,
    ST_MAP_CREATOR = 2,
    ST_DEBUG = 3,

    ST_INVALID
};

struct main_scene_t : ux::scene_t {
    void init() override;
    void subscribe_to_events(vkph::listener_t listener) override;
    void tick(frame_command_buffers_t *cmdbufs, vkph::state_t *state) override;
    void handle_event(void *object, vkph::event_t *events) override;
    void prepare_for_binding(vkph::state_t *state) override;
    void prepare_for_unbinding(vkph::state_t *state) override;

private:
    void handle_input(vkph::state_t *state);

    fixed_premade_scene_t structure_;
};

struct play_scene_t : ux::scene_t {
    void init() override;
    void subscribe_to_events(vkph::listener_t listener) override;
    void tick(frame_command_buffers_t *cmdbufs, vkph::state_t *state) override;
    void handle_event(void *object, vkph::event_t *events) override;
    void prepare_for_binding(vkph::state_t *state) override;
    void prepare_for_unbinding(vkph::state_t *state) override;
    
private:
    void handle_input(vkph::state_t *state);
    void calculate_pos_and_dir(vkph::player_t *player, vector3_t *position, vector3_t *direction);

    enum submode_t {
        S_MENU,
        S_IN_GAME,
        S_PAUSE,
        S_INVALID
    };

    submode_t submode_;
};

struct map_creator_scene_t : ux::scene_t {
    void init() override;
    void subscribe_to_events(vkph::listener_t listener) override;
    void tick(frame_command_buffers_t *cmdbufs, vkph::state_t *state) override;
    void handle_event(void *object, vkph::event_t *events) override;
    void prepare_for_binding(vkph::state_t *state) override;
    void prepare_for_unbinding(vkph::state_t *state) override;

private:
    bool push_edit_character(char c);
    void parse_and_generate_sphere(vkph::generation_type_t type, const char *str, uint32_t str_len, vkph::state_t *state);
    void parse_and_generate_hollow_sphere(vkph::generation_type_t type, const char *str, uint32_t str_len, vkph::state_t *state);
    void parse_and_execute_command(const char *str, uint32_t str_len, vkph::state_t *state);
    void handle_input(vkph::state_t *state);

    enum submode_t {
        S_IN_GAME,
        S_PAUSE,
        S_INVALID
    };

    submode_t submode_;
    vkph::map_t *map_;
    bool need_to_save_;
    bool started_command_;

    static constexpr uint32_t EDIT_BUFFER_MAX_CHAR_COUNT_ = 20;

    uint32_t edit_char_count_;
    char edit_buffer_[EDIT_BUFFER_MAX_CHAR_COUNT_] = {};
    vkph::voxel_color_t current_color_;
    bool display_text_in_minibuffer_;
};

struct debug_scene_t : ux::scene_t {
    void init() override;
    void subscribe_to_events(vkph::listener_t listener) override;
    void tick(frame_command_buffers_t *cmdbufs, vkph::state_t *state) override;
    void handle_event(void *object, vkph::event_t *events) override;
    void prepare_for_binding(vkph::state_t *state) override;
    void prepare_for_unbinding(vkph::state_t *state) override;
private:
    int32_t main_player_id_;
};

}
