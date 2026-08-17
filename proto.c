#include "proto.h"

#include <errno.h>
#include <poll.h>
#include <string.h>
#include <unistd.h>

void proto_conn_init(Conn *c, int fd)
{
    memset(c, 0, sizeof(*c));
    c->fd = fd;
}

void proto_color_from_name(const char *name, uint8_t *r, uint8_t *g, uint8_t *b)
{
    unsigned h = 2166136261u;
    const char *p;
    for (p = name; p && *p; p++)
        h = (h ^ (unsigned char)*p) * 16777619u;
    *r = (uint8_t)(70 + (h & 127));
    *g = (uint8_t)(70 + ((h >> 8) & 127));
    *b = (uint8_t)(70 + ((h >> 16) & 127));
}

uint8_t proto_id_from_pid(void)
{
    uint8_t id = (uint8_t)(getpid() & 0xff);
    return id ? id : 1;
}

static int write_all(int fd, const void *p, int n)
{
    const uint8_t *b = p;
    int off = 0, waits = 0;
    while (off < n) {
        ssize_t w = write(fd, b + off, (size_t)(n - off));
        if (w < 0) {
            struct pollfd pfd;
            if (errno == EINTR)
                continue;
            if (errno != EAGAIN && errno != EWOULDBLOCK)
                return -1;
            if (off == 0)
                return -1;
            pfd.fd = fd;
            pfd.events = POLLOUT;
            if (poll(&pfd, 1, 20) <= 0 || ++waits > 8)
                return -1;
            continue;
        }
        if (w == 0)
            return -1;
        off += (int)w;
    }
    return n;
}

int proto_send(int fd, const Msg *m)
{
    uint8_t pkt[4 + PROTO_MAX];
    int body = 0;

    pkt[4 + body++] = m->id;
    switch (m->type) {
    case MSG_JOIN:
        memcpy(pkt + 4 + body, m->name, PROTO_NAME);
        body += PROTO_NAME;
        pkt[4 + body++] = m->r;
        pkt[4 + body++] = m->g;
        pkt[4 + body++] = m->b;
        break;
    case MSG_STATE:
        pkt[4 + body++] = (uint8_t)(m->x & 0xff);
        pkt[4 + body++] = (uint8_t)((m->x >> 8) & 0xff);
        pkt[4 + body++] = (uint8_t)(m->y & 0xff);
        pkt[4 + body++] = (uint8_t)((m->y >> 8) & 0xff);
        break;
    case MSG_WELCOME:
    case MSG_LEAVE:
        break;
    default:
        return -1;
    }
    pkt[0] = m->type;
    pkt[1] = 0;
    pkt[2] = (uint8_t)(body & 0xff);
    pkt[3] = (uint8_t)((body >> 8) & 0xff);
    return write_all(fd, pkt, 4 + body) == 4 + body ? 0 : -1;
}

static int16_t i16le(const uint8_t *p)
{
    return (int16_t)(p[0] | (p[1] << 8));
}

int proto_recv(Conn *c, Msg *out)
{
    int need, body;

    if (c->fd < 0)
        return -1;
    if (c->n < 4)
        need = 4 - c->n;
    else {
        body = c->buf[2] | (c->buf[3] << 8);
        if (body < 0 || body > PROTO_MAX)
            return -1;
        need = 4 + body - c->n;
    }
    if (need > 0) {
        ssize_t r = read(c->fd, c->buf + c->n, (size_t)need);
        if (r < 0) {
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
                return 0;
            return -1;
        }
        if (r == 0)
            return -1;
        c->n += (int)r;
        if (c->n < 4)
            return 0;
        body = c->buf[2] | (c->buf[3] << 8);
        if (body < 0 || body > PROTO_MAX)
            return -1;
        if (c->n < 4 + body)
            return 0;
    }
    body = c->buf[2] | (c->buf[3] << 8);
    memset(out, 0, sizeof(*out));
    out->type = c->buf[0];
    if (body < 1)
        return -1;
    out->id = c->buf[4];
    if (out->type == MSG_JOIN && body >= 1 + PROTO_NAME + 3) {
        memcpy(out->name, c->buf + 5, PROTO_NAME);
        out->name[PROTO_NAME - 1] = 0;
        out->r = c->buf[5 + PROTO_NAME];
        out->g = c->buf[6 + PROTO_NAME];
        out->b = c->buf[7 + PROTO_NAME];
    } else if (out->type == MSG_STATE && body >= 5) {
        out->x = i16le(c->buf + 5);
        out->y = i16le(c->buf + 7);
    }
    c->n = 0;
    return 1;
}
