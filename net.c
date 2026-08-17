#include "net.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>

int net_set_nonblock(int fd)
{
    int fl = fcntl(fd, F_GETFL, 0);
    if (fl < 0)
        return -1;
    return fcntl(fd, F_SETFL, fl | O_NONBLOCK);
}

void net_close(int fd)
{
    if (fd >= 0)
        close(fd);
}

int net_listen(uint16_t port)
{
    int fd, yes = 1;
    struct sockaddr_in a;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return -1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    a.sin_port = htons(port);
    if (bind(fd, (struct sockaddr *)&a, sizeof(a)) < 0 || listen(fd, 8) < 0) {
        close(fd);
        return -1;
    }
    net_set_nonblock(fd);
    return fd;
}

int net_accept(int listen_fd)
{
    int fd = accept(listen_fd, NULL, NULL);
    if (fd < 0)
        return -1;
    net_set_nonblock(fd);
    return fd;
}

int net_connect(const char *host, uint16_t port)
{
    char serv[16];
    struct addrinfo hints, *res, *p;
    int fd = -1, rc;

    snprintf(serv, sizeof(serv), "%u", (unsigned)port);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    rc = getaddrinfo(host, serv, &hints, &res);
    if (rc != 0) {
        fprintf(stderr, "connect: %s\n", gai_strerror(rc));
        return -1;
    }
    for (p = res; p; p = p->ai_next) {
        fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd < 0)
            continue;
        if (connect(fd, p->ai_addr, p->ai_addrlen) == 0)
            break;
        close(fd);
        fd = -1;
    }
    freeaddrinfo(res);
    if (fd >= 0)
        net_set_nonblock(fd);
    return fd;
}

int net_parse_hostport(const char *s, char *host, int hostn, uint16_t *port)
{
    const char *colon;
    int n;
    unsigned long p;
    char *end;

    if (!s || !host || hostn < 2)
        return -1;
    colon = strrchr(s, ':');
    if (!colon || colon == s)
        return -1;
    n = (int)(colon - s);
    if (n >= hostn)
        n = hostn - 1;
    memcpy(host, s, (size_t)n);
    host[n] = 0;
    p = strtoul(colon + 1, &end, 10);
    if (end == colon + 1 || *end || p == 0 || p > 65535)
        return -1;
    *port = (uint16_t)p;
    return 0;
}
