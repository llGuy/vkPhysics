#pragma once

class player_t {
public:
    player_t() = default;
private:
    const char *name;
    vector3_t position;
    vector3_t view_direction;
};
