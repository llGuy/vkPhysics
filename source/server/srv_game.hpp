#pragma once

void srv_game_init(struct event_submissions_t *events);
void srv_game_tick();
void spawn_player(uint32_t client_id);
