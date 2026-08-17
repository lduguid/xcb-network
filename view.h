#ifndef VIEW_H
#define VIEW_H

#include "proto.h"

enum {
    VKEY_LEFT = 1,
    VKEY_RIGHT,
    VKEY_UP,
    VKEY_DOWN,
    VKEY_QUIT
};

typedef struct View View;

View *view_open(const char *title, int w, int h);
void view_close(View *v);
int view_fd(const View *v);
int view_width(const View *v);
int view_height(const View *v);
/* Drain X events. Returns 0 if the window should close. */
int view_pump(View *v);
int view_key_down(const View *v, int key);
void view_draw(View *v, const Player *me, const Player *others, int n, const char *status);

#endif
