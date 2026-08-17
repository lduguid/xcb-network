#define _POSIX_C_SOURCE 200809L

#include "net.h"
#include "proto.h"
#include "sim.h"
#include "view.h"

#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* Client/server: this process only talks to the server. The server assigns
 * an id and relays everyone else's JOIN/STATE. */

static double now_sec(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + t.tv_nsec / 1e9;
}

int main(int argc, char **argv)
{
    View *view;
    Player me, others[PROTO_PLAYERS];
    Conn conn;
    int nothers = 0, fd, welcomed = 0;
    uint16_t port;
    char status[160], name[PROTO_NAME];
    const char *user, *host;
    double prev, send_t;
    Msg m;

    if (argc < 3) {
        fprintf(stderr,
                "client/server: connect to a relay (./server).\n"
                "  %s <host> <port>\n"
                "  ./server 4000\n"
                "  ./client 127.0.0.1 4000\n",
                argv[0]);
        return 1;
    }
    host = argv[1];
    port = (uint16_t)atoi(argv[2]);
    if (port == 0) {
        fprintf(stderr, "bad port\n");
        return 1;
    }
    signal(SIGPIPE, SIG_IGN);

    fd = net_connect(host, port);
    if (fd < 0) {
        fprintf(stderr, "could not connect to %s:%u\n", host, (unsigned)port);
        return 1;
    }
    proto_conn_init(&conn, fd);

    user = getenv("USER");
    snprintf(name, sizeof(name), "%s", user && user[0] ? user : "client");
    player_setup(&me, 0, name, 200, 200);

    memset(&m, 0, sizeof(m));
    m.type = MSG_JOIN;
    m.id = 0;
    memcpy(m.name, me.name, PROTO_NAME);
    m.r = me.r;
    m.g = me.g;
    m.b = me.b;
    proto_send(fd, &m);

    view = view_open("xcb-network client (C/S)", 800, 560);
    if (!view)
        return 1;
    me.x = 200;
    me.y = (int16_t)(view_height(view) / 2);

    prev = now_sec();
    send_t = 0;
    fprintf(stderr, "client: connected to %s:%u as %s\n", host, (unsigned)port, me.name);

    while (view_pump(view)) {
        struct pollfd pfd[2];
        double t, dt;
        int pr;

        pfd[0].fd = view_fd(view);
        pfd[0].events = POLLIN;
        pfd[1].fd = conn.fd;
        pfd[1].events = POLLIN;
        if (poll(pfd, 2, 16) < 0)
            break;

        while ((pr = proto_recv(&conn, &m)) == 1) {
            if (m.type == MSG_WELCOME) {
                me.id = m.id;
                welcomed = 1;
                fprintf(stderr, "client: welcomed id=%u\n", me.id);
            } else if (m.id != me.id)
                roster_apply_msg(others, &nothers, &m);
        }
        if (pr < 0) {
            fprintf(stderr, "client: server closed\n");
            break;
        }

        t = now_sec();
        dt = t - prev;
        prev = t;
        if (dt > 0.05)
            dt = 0.05;
        player_move(&me, view_key_down(view, VKEY_LEFT), view_key_down(view, VKEY_RIGHT),
                    view_key_down(view, VKEY_UP), view_key_down(view, VKEY_DOWN), (float)dt,
                    view_width(view), view_height(view));

        send_t += dt;
        if (welcomed && send_t >= 0.05) {
            send_t = 0;
            memset(&m, 0, sizeof(m));
            m.type = MSG_STATE;
            m.id = me.id;
            m.x = me.x;
            m.y = me.y;
            proto_send(conn.fd, &m);
        }

        snprintf(status, sizeof(status),
                 "CLIENT  %s:%u  id %u  others %d   arrows move   Q quit",
                 host, (unsigned)port, me.id, nothers);
        view_draw(view, &me, others, nothers, status);
    }

    memset(&m, 0, sizeof(m));
    m.type = MSG_LEAVE;
    m.id = me.id;
    proto_send(conn.fd, &m);
    net_close(conn.fd);
    view_close(view);
    return 0;
}
