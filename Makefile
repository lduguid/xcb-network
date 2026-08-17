CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2 -D_POSIX_C_SOURCE=200809L $(shell pkg-config --cflags x11)
LIBS = $(shell pkg-config --libs x11)

COMMON = proto.c net.c sim.c view.c proto.h net.h sim.h view.h

all: peer server client

peer: peer.c $(COMMON)
	$(CC) $(CFLAGS) -o $@ peer.c proto.c net.c sim.c view.c $(LIBS)

server: server.c proto.c net.c proto.h net.h
	$(CC) $(CFLAGS) -o $@ server.c proto.c net.c

client: client.c $(COMMON)
	$(CC) $(CFLAGS) -o $@ client.c proto.c net.c sim.c view.c $(LIBS)

clean:
	rm -f peer server client
