#include "view.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#define HUD 28

struct View {
    Display *dpy;
    Window win;
    GC gc;
    Pixmap back;
    Atom wm_delete;
    unsigned long rmask, gmask, bmask;
    int rshift, gshift, bshift;
    int w, h, screen;
    int keys[8];
    int focused;
    int running;
};

static int mask_shift(unsigned long m)
{
    int s = 0;
    if (!m)
        return 0;
    while ((m & 1ul) == 0) {
        m >>= 1;
        s++;
    }
    return s;
}

View *view_open(const char *title, int w, int h)
{
    View *v = calloc(1, sizeof(*v));
    XSizeHints hints;

    if (!v)
        return NULL;
    v->dpy = XOpenDisplay(NULL);
    if (!v->dpy) {
        fprintf(stderr, "cannot open X display\n");
        free(v);
        return NULL;
    }
    v->screen = DefaultScreen(v->dpy);
    {
        Visual *vis = DefaultVisual(v->dpy, v->screen);
        v->rmask = vis->red_mask;
        v->gmask = vis->green_mask;
        v->bmask = vis->blue_mask;
        v->rshift = mask_shift(v->rmask);
        v->gshift = mask_shift(v->gmask);
        v->bshift = mask_shift(v->bmask);
    }
    v->w = w > 320 ? w : 320;
    v->h = h > 200 ? h : 200;
    v->focused = 1;
    v->win = XCreateSimpleWindow(v->dpy, RootWindow(v->dpy, v->screen), 80, 80, (unsigned)v->w,
                                 (unsigned)v->h, 1, BlackPixel(v->dpy, v->screen),
                                 BlackPixel(v->dpy, v->screen));
    v->gc = XCreateGC(v->dpy, v->win, 0, NULL);
    v->back = XCreatePixmap(v->dpy, v->win, (unsigned)v->w, (unsigned)v->h,
                            (unsigned)DefaultDepth(v->dpy, v->screen));
    hints.flags = PMinSize;
    hints.min_width = 320;
    hints.min_height = 200;
    XSetWMNormalHints(v->dpy, v->win, &hints);
    XStoreName(v->dpy, v->win, title ? title : "xcb-network");
    v->wm_delete = XInternAtom(v->dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(v->dpy, v->win, &v->wm_delete, 1);
    XSelectInput(v->dpy, v->win,
                 KeyPressMask | KeyReleaseMask | ExposureMask | StructureNotifyMask | FocusChangeMask);
    XMapWindow(v->dpy, v->win);
    v->running = 1;
    return v;
}

void view_close(View *v)
{
    if (!v)
        return;
    if (v->dpy) {
        XFreePixmap(v->dpy, v->back);
        XFreeGC(v->dpy, v->gc);
        XDestroyWindow(v->dpy, v->win);
        XCloseDisplay(v->dpy);
    }
    free(v);
}

int view_fd(const View *v)
{
    return v ? ConnectionNumber(v->dpy) : -1;
}

int view_width(const View *v)
{
    return v ? v->w : 0;
}

int view_height(const View *v)
{
    return v ? v->h : 0;
}

int view_key_down(const View *v, int key)
{
    if (!v || key <= 0 || key >= 8)
        return 0;
    return v->keys[key];
}

static int map_key(KeySym ks)
{
    if (ks == XK_Left || ks == XK_a || ks == XK_A)
        return VKEY_LEFT;
    if (ks == XK_Right || ks == XK_d || ks == XK_D)
        return VKEY_RIGHT;
    if (ks == XK_Up || ks == XK_w || ks == XK_W)
        return VKEY_UP;
    if (ks == XK_Down || ks == XK_s || ks == XK_S)
        return VKEY_DOWN;
    if (ks == XK_Escape || ks == XK_q || ks == XK_Q)
        return VKEY_QUIT;
    return 0;
}

static int keymap_down(const char km[32], Display *dpy, KeySym ks)
{
    KeyCode kc = XKeysymToKeycode(dpy, ks);
    if (!kc)
        return 0;
    return (km[kc / 8] & (1 << (kc % 8))) != 0;
}

/* Per-window key state. XQueryKeymap is used only while focused so a lost
 * KeyRelease cannot pin left/up and trap the square in a corner. FocusOut
 * clears the array: otherwise clicking the other peer window while holding
 * arrows leaves those keys down forever. */
static void sync_keys(View *v)
{
    char km[32];
    if (!v->focused) {
        memset(v->keys, 0, sizeof(v->keys));
        return;
    }
    XQueryKeymap(v->dpy, km);
    v->keys[VKEY_LEFT] = keymap_down(km, v->dpy, XK_Left) || keymap_down(km, v->dpy, XK_a);
    v->keys[VKEY_RIGHT] = keymap_down(km, v->dpy, XK_Right) || keymap_down(km, v->dpy, XK_d);
    v->keys[VKEY_UP] = keymap_down(km, v->dpy, XK_Up) || keymap_down(km, v->dpy, XK_w);
    v->keys[VKEY_DOWN] = keymap_down(km, v->dpy, XK_Down) || keymap_down(km, v->dpy, XK_s);
}

int view_pump(View *v)
{
    XEvent ev;
    if (!v || !v->running)
        return 0;
    while (XPending(v->dpy)) {
        XNextEvent(v->dpy, &ev);
        if (ev.type == KeyPress || ev.type == KeyRelease) {
            KeySym ks = XLookupKeysym(&ev.xkey, 0);
            int k = map_key(ks);
            if (k == VKEY_QUIT && ev.type == KeyPress)
                v->running = 0;
            else if (k > 0 && k < 8 && v->focused)
                v->keys[k] = (ev.type == KeyPress);
        } else if (ev.type == FocusIn) {
            if (ev.xfocus.mode != NotifyGrab)
                v->focused = 1;
        } else if (ev.type == FocusOut) {
            if (ev.xfocus.mode != NotifyGrab && ev.xfocus.mode != NotifyUngrab) {
                v->focused = 0;
                memset(v->keys, 0, sizeof(v->keys));
            }
        } else if (ev.type == ConfigureNotify) {
            int nw = ev.xconfigure.width, nh = ev.xconfigure.height;
            if (nw >= 64 && nh >= 64 && (nw != v->w || nh != v->h)) {
                v->w = nw;
                v->h = nh;
                XFreePixmap(v->dpy, v->back);
                v->back = XCreatePixmap(v->dpy, v->win, (unsigned)v->w, (unsigned)v->h,
                                        (unsigned)DefaultDepth(v->dpy, v->screen));
            }
        } else if (ev.type == DestroyNotify)
            v->running = 0;
        else if (ev.type == ClientMessage && (Atom)ev.xclient.data.l[0] == v->wm_delete)
            v->running = 0;
    }
    sync_keys(v);
    return v->running;
}

static unsigned long rgb(const View *v, int r, int g, int b)
{
    unsigned long p = 0;
    if (r < 0)
        r = 0;
    if (g < 0)
        g = 0;
    if (b < 0)
        b = 0;
    if (r > 255)
        r = 255;
    if (g > 255)
        g = 255;
    if (b > 255)
        b = 255;
    if (v->rmask)
        p |= ((unsigned long)r << v->rshift) & v->rmask;
    if (v->gmask)
        p |= ((unsigned long)g << v->gshift) & v->gmask;
    if (v->bmask)
        p |= ((unsigned long)b << v->bshift) & v->bmask;
    return p;
}

static void fill(View *v, int x, int y, int w, int h, int r, int g, int b)
{
    XSetForeground(v->dpy, v->gc, rgb(v, r, g, b));
    XFillRectangle(v->dpy, v->back, v->gc, x, y, (unsigned)w, (unsigned)h);
}

static void text(View *v, int x, int y, const char *s, int r, int g, int b)
{
    XSetForeground(v->dpy, v->gc, rgb(v, r, g, b));
    XDrawString(v->dpy, v->back, v->gc, x, y, s, (int)strlen(s));
}

void view_draw(View *v, const Player *me, const Player *others, int n, const char *status)
{
    int i;
    const int sz = 28;

    if (!v)
        return;
    fill(v, 0, 0, v->w, v->h, 18, 20, 26);
    fill(v, 0, 0, v->w, HUD, 10, 12, 16);
    text(v, 10, 18, status ? status : "", 200, 210, 220);

    for (i = 0; i < n; i++) {
        char lab[20];
        if (!others[i].alive)
            continue;
        fill(v, others[i].x, others[i].y, sz, sz, others[i].r, others[i].g, others[i].b);
        snprintf(lab, sizeof(lab), "%s", others[i].name);
        text(v, others[i].x, others[i].y - 4, lab, 230, 230, 230);
    }
    if (me && me->alive) {
        fill(v, me->x, me->y, sz, sz, me->r, me->g, me->b);
        XSetForeground(v->dpy, v->gc, rgb(v, 255, 255, 255));
        XDrawRectangle(v->dpy, v->back, v->gc, me->x, me->y, (unsigned)sz, (unsigned)sz);
        text(v, me->x, me->y - 4, me->name, 255, 255, 255);
    }
    XCopyArea(v->dpy, v->back, v->win, v->gc, 0, 0, (unsigned)v->w, (unsigned)v->h, 0, 0);
    XFlush(v->dpy);
}
