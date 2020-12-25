#include "common/constant.hpp"
#include "common/containers.hpp"
#include "map.hpp"
#include "game.hpp"
#include "event.hpp"
#include "chunk.hpp"
#include "player.hpp"
#include <math.h>

game_t *g_game;

void game_allocate() {
    g_game = FL_MALLOC(game_t, 1);
}

