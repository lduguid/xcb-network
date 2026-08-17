#ifndef PROTO_H
#define PROTO_H

#include <stdint.h>

#define PROTO_MAX 64
#define PROTO_NAME 16
#define PROTO_PLAYERS 8

enum {
    MSG_JOIN = 1,
    MSG_WELCOME = 2,
    MSG_STATE = 3,
    MSG_LEAVE = 4
};

typedef struct {
    uint8_t id;
    char name[PROTO_NAME];
    uint8_t r, g, b;
    int16_t x, y; /* last local sim / last network pose */
    float px, py; /* drawn pose; remote cubes ease toward x, y */
    int alive;
} Player;

typedef struct {
    uint8_t type;
    uint8_t id;
    char name[PROTO_NAME];
    uint8_t r, g, b;
    int16_t x, y;
} Msg;

typedef struct {
    int fd;
    uint8_t buf[4 + PROTO_MAX];
    int n;
} Conn;

int proto_send(int fd, const Msg *m);
/* 1 = message, 0 = need more, -1 = closed/error */
int proto_recv(Conn *c, Msg *out);
void proto_conn_init(Conn *c, int fd);
void proto_color_from_name(const char *name, uint8_t *r, uint8_t *g, uint8_t *b);
uint8_t proto_id_from_pid(void);

#endif
