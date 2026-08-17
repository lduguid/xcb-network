#ifndef NET_H
#define NET_H

#include <stdint.h>

int net_listen(uint16_t port);
int net_accept(int listen_fd);
int net_connect(const char *host, uint16_t port);
int net_set_nonblock(int fd);
void net_close(int fd);
int net_parse_hostport(const char *s, char *host, int hostn, uint16_t *port);

#endif
