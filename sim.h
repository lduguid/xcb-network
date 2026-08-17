#ifndef SIM_H
#define SIM_H

#include "proto.h"

void player_setup(Player *p, uint8_t id, const char *name, int x, int y);
void player_move(Player *p, int left, int right, int up, int down, float dt, int w, int h);
Player *roster_find(Player *list, int n, uint8_t id);
void roster_upsert(Player *list, int *n, const Player *src);
void roster_remove(Player *list, int *n, uint8_t id);
void roster_apply_msg(Player *list, int *n, const Msg *m);

#endif
