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

/* P2P: each process listens and may connect to one other peer.
 * Every connected socket is an equal: JOIN + STATE go both ways. */

#define MAX_LINK 4

static double now_sec(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + t.tv_nsec / 1e9;
}

static void send_join(int fd, const Player *me)
{
    Msg m;
    memset(&m, 0, sizeof(m));
    m.type = MSG_JOIN;
    m.id = me->id;
    memcpy(m.name, me->name, PROTO_NAME);
    m.r = me->r;
    m.g = me->g;
    m.b = me->b;
    proto_send(fd, &m);
}

static void send_state(int fd, const Player *me)
{
    Msg m;
    memset(&m, 0, sizeof(m));
    m.type = MSG_STATE;
    m.id = me->id;
    m.x = me->x;
    m.y = me->y;
    proto_send(fd, &m);
}

static void broadcast(Conn *links, int n, const Msg *m)
{
    int i;
    for (i = 0; i < n; i++) {
        if (links[i].fd >= 0)
            proto_send(links[i].fd, m);
    }
}

int main(int argc, char **argv)
{
    View *view;
    Player me, others[PROTO_PLAYERS];
    Conn links[MAX_LINK];
    int nlink = 0, nothers = 0, listen_fd, i;
    uint16_t port;
    char status[160], name[PROTO_NAME];
    const char *user;
    double prev, send_t;
    Msg m;

    if (argc < 2) {
        fprintf(stderr,
                "peer-to-peer: each window is equal; no server.\n"
                "  %s <listen-port> [host:port]\n"
                "  terminal A:  ./peer 4000\n"
                "  terminal B:  ./peer 4001 127.0.0.1:4000\n",
                argv[0]);
        return 1;
    }
    port = (uint16_t)atoi(argv[1]);
    if (port == 0) {
        fprintf(stderr, "bad port\n");
        return 1;
    }
    signal(SIGPIPE, SIG_IGN);

    listen_fd = net_listen(port);
    if (listen_fd < 0) {
        perror("listen");
        return 1;
    }

    user = getenv("USER");
    snprintf(name, sizeof(name), "%s-%u", user && user[0] ? user : "peer", (unsigned)port);
    player_setup(&me, proto_id_from_pid(), name, 120, 160);

    if (argc >= 3) {
        char host[128];
        uint16_t pport;
        int fd;
        if (net_parse_hostport(argv[2], host, sizeof(host), &pport) != 0) {
            fprintf(stderr, "use host:port (got %s)\n", argv[2]);
            return 1;
        }
        fd = net_connect(host, pport);
        if (fd < 0) {
            fprintf(stderr, "could not connect to %s:%u\n", host, (unsigned)pport);
            return 1;
        }
        proto_conn_init(&links[nlink], fd);
        send_join(fd, &me);
        send_state(fd, &me);
        nlink++;
        fprintf(stderr, "p2p: connected to %s:%u\n", host, (unsigned)pport);
    }

    view = view_open("xcb-network peer (P2P)", 800, 560);
    if (!view)
        return 1;
    me.x = 80;
    me.y = (int16_t)(view_height(view) / 2);

    prev = now_sec();
    send_t = 0;
    fprintf(stderr, "p2p: listening on %u  id=%u  (%s)\n", (unsigned)port, me.id, me.name);

    while (view_pump(view)) {
        struct pollfd pfd[2 + MAX_LINK];
        int np = 0, xfd = view_fd(view);
        double t, dt;

        pfd[np].fd = xfd;
        pfd[np].events = POLLIN;
        np++;
        pfd[np].fd = listen_fd;
        pfd[np].events = POLLIN;
        np++;
        for (i = 0; i < nlink; i++) {
            pfd[np].fd = links[i].fd;
            pfd[np].events = POLLIN;
            np++;
        }
        poll(pfd, (nfds_t)np, 16);

        if (pfd[1].revents & POLLIN) {
            int fd = net_accept(listen_fd);
            if (fd >= 0 && nlink < MAX_LINK) {
                proto_conn_init(&links[nlink], fd);
                send_join(fd, &me);
                send_state(fd, &me);
                nlink++;
                fprintf(stderr, "p2p: accepted peer (links=%d)\n", nlink);
            } else if (fd >= 0)
                net_close(fd);
        }
        for (i = 0; i < nlink; i++) {
            int pr;
            while ((pr = proto_recv(&links[i], &m)) == 1) {
                if (m.id != me.id)
                    roster_apply_msg(others, &nothers, &m);
            }
            if (pr < 0) {
                fprintf(stderr, "p2p: peer disconnected\n");
                net_close(links[i].fd);
                links[i] = links[nlink - 1];
                nlink--;
                i--;
            }
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
        if (send_t >= 0.05) {
            send_t = 0;
            memset(&m, 0, sizeof(m));
            m.type = MSG_STATE;
            m.id = me.id;
            m.x = me.x;
            m.y = me.y;
            broadcast(links, nlink, &m);
        }

        snprintf(status, sizeof(status),
                 "P2P  listen :%u  links %d  others %d   arrows move   Q quit",
                 (unsigned)port, nlink, nothers);
        view_draw(view, &me, others, nothers, status);
    }

    memset(&m, 0, sizeof(m));
    m.type = MSG_LEAVE;
    m.id = me.id;
    broadcast(links, nlink, &m);
    for (i = 0; i < nlink; i++)
        net_close(links[i].fd);
    net_close(listen_fd);
    view_close(view);
    return 0;
}
