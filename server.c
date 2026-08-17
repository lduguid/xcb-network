#define _POSIX_C_SOURCE 200809L

#include "net.h"
#include "proto.h"

#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Relay: no window. Assigns ids, forwards JOIN/STATE/LEAVE to everyone else. */

#define MAX_CL 8

typedef struct {
    Conn conn;
    uint8_t id;
    uint8_t r, g, b;
    char name[PROTO_NAME];
    int joined;
} Client;

static void send_to(Client *cl, const Msg *m)
{
    if (cl->conn.fd >= 0)
        proto_send(cl->conn.fd, m);
}

static void relay(Client *cls, int n, int from, const Msg *m)
{
    int i;
    for (i = 0; i < n; i++) {
        if (i != from && cls[i].joined)
            send_to(&cls[i], m);
    }
}

int main(int argc, char **argv)
{
    Client cls[MAX_CL];
    int n = 0, listen_fd, i, next_id = 1;
    uint16_t port;
    Msg m;

    if (argc < 2) {
        fprintf(stderr, "usage: %s <port>\n  then: ./client 127.0.0.1 <port>\n", argv[0]);
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
    memset(cls, 0, sizeof(cls));
    fprintf(stderr, "server: listening on %u  (client/server relay, no window)\n", (unsigned)port);

    for (;;) {
        struct pollfd pfd[1 + MAX_CL];
        int np = 0;
        pfd[np].fd = listen_fd;
        pfd[np].events = POLLIN;
        np++;
        for (i = 0; i < n; i++) {
            pfd[np].fd = cls[i].conn.fd;
            pfd[np].events = POLLIN;
            np++;
        }
        if (poll(pfd, (nfds_t)np, -1) < 0)
            break;

        if (pfd[0].revents & POLLIN) {
            int fd = net_accept(listen_fd);
            if (fd >= 0 && n < MAX_CL) {
                proto_conn_init(&cls[n].conn, fd);
                cls[n].id = 0;
                cls[n].joined = 0;
                cls[n].name[0] = 0;
                n++;
                fprintf(stderr, "server: accept  sockets=%d\n", n);
            } else if (fd >= 0)
                net_close(fd);
        }

        for (i = 0; i < n; i++) {
            int pr;
            while ((pr = proto_recv(&cls[i].conn, &m)) == 1) {
                if (m.type == MSG_JOIN && !cls[i].joined) {
                    int j;
                    if (next_id == 0 || next_id > 250)
                        next_id = 1;
                    cls[i].id = (uint8_t)next_id++;
                    memcpy(cls[i].name, m.name, PROTO_NAME);
                    cls[i].name[PROTO_NAME - 1] = 0;
                    cls[i].r = m.r;
                    cls[i].g = m.g;
                    cls[i].b = m.b;
                    cls[i].joined = 1;
                    memset(&m, 0, sizeof(m));
                    m.type = MSG_WELCOME;
                    m.id = cls[i].id;
                    send_to(&cls[i], &m);
                    for (j = 0; j < n; j++) {
                        if (j == i || !cls[j].joined)
                            continue;
                        memset(&m, 0, sizeof(m));
                        m.type = MSG_JOIN;
                        m.id = cls[j].id;
                        memcpy(m.name, cls[j].name, PROTO_NAME);
                        m.r = cls[j].r;
                        m.g = cls[j].g;
                        m.b = cls[j].b;
                        proto_send(cls[i].conn.fd, &m);
                    }
                    memset(&m, 0, sizeof(m));
                    m.type = MSG_JOIN;
                    m.id = cls[i].id;
                    memcpy(m.name, cls[i].name, PROTO_NAME);
                    m.r = cls[i].r;
                    m.g = cls[i].g;
                    m.b = cls[i].b;
                    relay(cls, n, i, &m);
                    fprintf(stderr, "server: join id=%u (%s)\n", cls[i].id, cls[i].name);
                } else if (cls[i].joined && (m.type == MSG_STATE || m.type == MSG_LEAVE)) {
                    m.id = cls[i].id;
                    relay(cls, n, i, &m);
                }
            }
            if (pr < 0) {
                if (cls[i].joined) {
                    Msg leave;
                    memset(&leave, 0, sizeof(leave));
                    leave.type = MSG_LEAVE;
                    leave.id = cls[i].id;
                    relay(cls, n, i, &leave);
                    fprintf(stderr, "server: leave id=%u (%s)\n", cls[i].id, cls[i].name);
                }
                net_close(cls[i].conn.fd);
                cls[i] = cls[n - 1];
                n--;
                i--;
            }
        }
    }
    for (i = 0; i < n; i++)
        net_close(cls[i].conn.fd);
    net_close(listen_fd);
    return 0;
}
