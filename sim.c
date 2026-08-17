#include "sim.h"

#include <stdio.h>
#include <string.h>

void player_setup(Player *p, uint8_t id, const char *name, int x, int y)
{
    memset(p, 0, sizeof(*p));
    p->id = id;
    p->alive = 1;
    p->x = (int16_t)x;
    p->y = (int16_t)y;
    snprintf(p->name, PROTO_NAME, "%s", name ? name : "anon");
    proto_color_from_name(p->name, &p->r, &p->g, &p->b);
}

void player_move(Player *p, int left, int right, int up, int down, float dt, int w, int h)
{
    float sp = 240.0f;
    float x = p->x, y = p->y;
    if (left)
        x -= sp * dt;
    if (right)
        x += sp * dt;
    if (up)
        y -= sp * dt;
    if (down)
        y += sp * dt;
    {
        float xmin = 8, ymin = 36;
        float xmax = (float)(w - 36), ymax = (float)(h - 36);
        if (xmax < xmin)
            xmax = xmin;
        if (ymax < ymin)
            ymax = ymin;
        if (x < xmin)
            x = xmin;
        if (y < ymin)
            y = ymin;
        if (x > xmax)
            x = xmax;
        if (y > ymax)
            y = ymax;
    }
    p->x = (int16_t)x;
    p->y = (int16_t)y;
}

Player *roster_find(Player *list, int n, uint8_t id)
{
    int i;
    for (i = 0; i < n; i++) {
        if (list[i].alive && list[i].id == id)
            return &list[i];
    }
    return NULL;
}

void roster_upsert(Player *list, int *n, const Player *src)
{
    Player *e = roster_find(list, *n, src->id);
    if (e) {
        *e = *src;
        e->alive = 1;
        return;
    }
    if (*n >= PROTO_PLAYERS)
        return;
    list[*n] = *src;
    list[*n].alive = 1;
    (*n)++;
}

void roster_remove(Player *list, int *n, uint8_t id)
{
    int i;
    for (i = 0; i < *n; i++) {
        if (list[i].id == id) {
            list[i] = list[*n - 1];
            (*n)--;
            return;
        }
    }
}

void roster_apply_msg(Player *list, int *n, const Msg *m)
{
    Player *e, tmp;
    if (m->type == MSG_LEAVE) {
        roster_remove(list, n, m->id);
        return;
    }
    if (m->type == MSG_JOIN) {
        player_setup(&tmp, m->id, m->name, 80, 80);
        tmp.r = m->r;
        tmp.g = m->g;
        tmp.b = m->b;
        roster_upsert(list, n, &tmp);
        return;
    }
    if (m->type == MSG_STATE) {
        e = roster_find(list, *n, m->id);
        if (e) {
            e->x = m->x;
            e->y = m->y;
        } else {
            player_setup(&tmp, m->id, "peer", m->x, m->y);
            roster_upsert(list, n, &tmp);
        }
    }
}
