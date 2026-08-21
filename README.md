# xcb-network

You are writing a tiny multiplayer toy: a named colored square on an X11 window, moved with arrows or WASD. The “platform” is a **length-prefixed TCP protocol** plus a nonblocking I/O loop, not a game engine.

The same `JOIN` / `WELCOME` / `STATE` / `LEAVE` messages run two topologies. Nothing here is wired into xcb-canvas yet; steal `proto.c` / `sim.c` if you later want canvas multiplayer.

Linux / WSL only (`pkg-config x11`, POSIX sockets). The server has no window.

## Build

```bash
make
```

## The wire

Every message is a 4-byte header (`type`, pad, little-endian length) plus a short body (`proto.h`). Sockets are **nonblocking**. A graphical process polls the X connection together with its TCP fds — do not block in `recv`.

`sim.h` is local motion and the other-player roster. Remote cubes **ease** toward the last `STATE`; your own square is simulated immediately.

## Peer to peer

Each process is equal. Every node listens, and may also connect to one other node. There is no host that assigns ids; each peer picks one from its pid (`proto_id_from_pid`).

```bash
./peer 4000
./peer 4001 127.0.0.1:4000
./peer 4002 127.0.0.1:4000
```

JOIN and STATE travel both ways on the TCP link. If you add a third message type, both ends must speak it.

## Client / server

The server is a console relay: accept, `WELCOME` with an id, forward JOIN / STATE / LEAVE to everyone else. Clients never talk to each other.

```bash
./server 4000
./client 127.0.0.1 4000
```

## Keys

Arrows or WASD move. Q, Esc, or close quits.

## Files you touch

| File | Your job |
|------|----------|
| `proto.c` | Pack/unpack. Keep `PROTO_MAX` small. |
| `net.c` | Listen / accept / connect, still nonblocking. |
| `sim.c` | Speed, roster, smoothing. |
| `view.c` | Window, keys, double-buffer. |
| `peer.c` / `client.c` / `server.c` | Who initiates the TCP link. |
