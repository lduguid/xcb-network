# xcb-network

Two small X11 demos of TCP networking, in the same family as `xcb-clock`, `xcb-grid`, and `xcb-canvas`. This project is independent of the canvas. Later the same ideas could feed multiplayer into canvas games; nothing here is wired into that platform yet.

Both examples share one framed protocol (`JOIN`, `WELCOME`, `STATE`, `LEAVE`) and the same window: a named colored square you move with the arrow keys or WASD.

## Build

```bash
make
```

Needs X11 (`pkg-config x11`) and a POSIX sockets stack. The server has no window.

## Peer to peer

Each process is equal. Every node listens, and may also connect to one other node. JOIN and STATE travel both ways on the TCP link. There is no host that assigns ids; each peer picks one from its pid.

```bash
# terminal A
./peer 4000

# terminal B
./peer 4001 127.0.0.1:4000
```

A third window can join either listener:

```bash
./peer 4002 127.0.0.1:4000
```

## Client / server

The server is a console relay. It accepts clients, assigns ids with `WELCOME`, and forwards JOIN / STATE / LEAVE to everyone else. Clients only talk to the server, never to each other.

```bash
# terminal A
./server 4000

# terminals B and C
./client 127.0.0.1 4000
```

## Keys

| Key | Action |
|-----|--------|
| Arrows or WASD | Move your square |
| Q or Esc | Quit |
| Window close | Quit |

## Layout

| File | Role |
|------|------|
| `proto.c` | Length-prefixed messages over a byte stream |
| `net.c` | Nonblocking IPv4 listen / accept / connect |
| `sim.c` | Movement and the other-player roster |
| `view.c` | Xlib window, keys, double-buffer |
| `peer.c` | Equal peers, each with a window |
| `server.c` | Headless relay |
| `client.c` | Window that talks only to the server |

The wire format is a 4-byte header (`type`, pad, little-endian length) plus a short body. Sockets are nonblocking; each graphical process polls the X connection together with its TCP fds.
