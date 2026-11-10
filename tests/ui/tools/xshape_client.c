/*
 * SPDX-License-Identifier: MIT
 * xshape_client.c - An X11 window for testing how a compositor treats the
 * X Shape extension, as Wine's layered windows (Proton's focus overlays)
 * use it: a 32-bit window whose bounding shape is a ring, with the inside
 * left out of the shape and fully transparent.
 *
 *   fluxland-test-xshape [--x N] [--y N] [--w N] [--h N] [--ring|--wine]
 *       [--managed] [--fill] [--popup] [--title T] [--class C] [--seconds N]
 *
 * It is override-redirect unless --managed; --fill makes the inside opaque
 * green (else it is transparent), which only a shape can then take out; --class
 * sets WM_CLASS (instance and class); --popup makes it a popup menu
 * (_NET_WM_WINDOW_TYPE_POPUP_MENU).  --ring is a 5 pixel ring,
 * --wine the four rectangles Wine made for an 85x33 overlay (which reach
 * beyond the window), none an unshaped window.  Prints "ready <window id>"
 * when drawn.  SIGUSR1 raises the window to the top of the X stack, SIGUSR2
 * toggles its shape on and off, SIGTERM leaves.
 */

#define _DEFAULT_SOURCE
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <xcb/shape.h>
#include <xcb/xcb.h>

#define RING 5
#define INSIDE 0xffe0ffffu	/* opaque light cyan: the ring itself */

static volatile sig_atomic_t want_raise, want_toggle, want_quit;

static void
on_signal(int sig)
{
	if (sig == SIGUSR1)
		want_raise = 1;
	else if (sig == SIGUSR2)
		want_toggle = 1;
	else
		want_quit = 1;
}

static int
number(int argc, char **argv, const char *name, int fallback)
{
	for (int i = 1; i + 1 < argc; i++)
		if (!strcmp(argv[i], name))
			return atoi(argv[i + 1]);
	return fallback;
}

static int
flag(int argc, char **argv, const char *name)
{
	for (int i = 1; i < argc; i++)
		if (!strcmp(argv[i], name))
			return 1;
	return 0;
}

int
main(int argc, char **argv)
{
	int x = number(argc, argv, "--x", 100), y = number(argc, argv, "--y", 100);
	int w = number(argc, argv, "--w", 85), h = number(argc, argv, "--h", 33);
	int secs = number(argc, argv, "--seconds", 60);
	int managed = flag(argc, argv, "--managed");
	int wine = flag(argc, argv, "--wine");
	int ring = wine || flag(argc, argv, "--ring");
	int fill = flag(argc, argv, "--fill");
	int popup = flag(argc, argv, "--popup");
	const char *title = "xshape", *wm_class = NULL;
	for (int i = 1; i + 1 < argc; i++) {
		if (!strcmp(argv[i], "--title"))
			title = argv[i + 1];
		if (!strcmp(argv[i], "--class"))
			wm_class = argv[i + 1];
	}

	signal(SIGUSR1, on_signal);
	signal(SIGUSR2, on_signal);
	signal(SIGTERM, on_signal);

	xcb_connection_t *c = xcb_connect(NULL, NULL);
	if (xcb_connection_has_error(c)) {
		fprintf(stderr, "no X display\n");
		return 1;
	}
	xcb_screen_t *s = xcb_setup_roots_iterator(xcb_get_setup(c)).data;
	xcb_visualid_t vis = 0;
	for (xcb_depth_iterator_t di = xcb_screen_allowed_depths_iterator(s);
	     di.rem; xcb_depth_next(&di)) {
		if (di.data->depth != 32)
			continue;
		xcb_visualtype_iterator_t vi = xcb_depth_visuals_iterator(di.data);
		if (vi.rem)
			vis = vi.data->visual_id;
	}
	if (!vis) {
		fprintf(stderr, "no 32-bit visual\n");
		return 1;
	}

	xcb_colormap_t cmap = xcb_generate_id(c);
	xcb_create_colormap(c, XCB_COLORMAP_ALLOC_NONE, cmap, s->root, vis);
	xcb_window_t win = xcb_generate_id(c);
	uint32_t vals[] = { 0, managed ? 0 : 1, cmap };
	xcb_create_window(c, 32, win, s->root, x, y, w, h, 0,
		XCB_WINDOW_CLASS_INPUT_OUTPUT, vis,
		XCB_CW_BORDER_PIXEL | XCB_CW_OVERRIDE_REDIRECT | XCB_CW_COLORMAP,
		vals);
	xcb_change_property(c, XCB_PROP_MODE_REPLACE, win, XCB_ATOM_WM_NAME,
		XCB_ATOM_STRING, 8, strlen(title), title);

	if (wm_class) {
		char buf[256];
		int len = snprintf(buf, sizeof(buf), "%s%c%s%c", wm_class, 0,
			wm_class, 0);
		xcb_change_property(c, XCB_PROP_MODE_REPLACE, win,
			XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 8, len, buf);
	}

	if (popup) {
		const char *names[2] = { "_NET_WM_WINDOW_TYPE",
			"_NET_WM_WINDOW_TYPE_POPUP_MENU" };
		xcb_atom_t atoms[2];
		for (int i = 0; i < 2; i++) {
			xcb_intern_atom_reply_t *r = xcb_intern_atom_reply(c,
				xcb_intern_atom(c, 0, strlen(names[i]), names[i]),
				NULL);
			atoms[i] = r ? r->atom : XCB_ATOM_NONE;
			free(r);
		}
		xcb_change_property(c, XCB_PROP_MODE_REPLACE, win, atoms[0],
			XCB_ATOM_ATOM, 32, 1, &atoms[1]);
	}

	xcb_rectangle_t rects[4];
	int nrects = 0;
	if (wine) {
		rects[0] = (xcb_rectangle_t){ 0, 0, 128, 5 };
		rects[1] = (xcb_rectangle_t){ 0, 5, 5, 23 };
		rects[2] = (xcb_rectangle_t){ 80, 5, 48, 23 };
		rects[3] = (xcb_rectangle_t){ 0, 28, 128, 100 };
		nrects = 4;
	} else if (ring) {
		rects[0] = (xcb_rectangle_t){ 0, 0, w, RING };
		rects[1] = (xcb_rectangle_t){ 0, RING, RING, h - 2 * RING };
		rects[2] = (xcb_rectangle_t){ w - RING, RING, RING, h - 2 * RING };
		rects[3] = (xcb_rectangle_t){ 0, h - RING, w, RING };
		nrects = 4;
	}
	int shaped = nrects > 0;
	if (shaped)
		xcb_shape_rectangles(c, XCB_SHAPE_SO_SET, XCB_SHAPE_SK_BOUNDING,
			XCB_CLIP_ORDERING_UNSORTED, win, 0, 0, nrects, rects);

	xcb_gcontext_t gc = xcb_generate_id(c);
	xcb_create_gc(c, gc, win, 0, NULL);
	uint32_t *px = calloc((size_t)w * h, 4);
	for (int j = 0; j < h; j++)
		for (int i = 0; i < w; i++) {
			int on_ring = i < RING || j < RING || i >= w - RING ||
				j >= h - RING;
			px[j * w + i] = on_ring ? INSIDE :
				fill ? 0xff00ff00u : 0x00000000u;
		}

	xcb_map_window(c, win);
	xcb_flush(c);
	usleep(200000);
	xcb_put_image(c, XCB_IMAGE_FORMAT_Z_PIXMAP, win, gc, w, h, 0, 0, 0, 32,
		(uint32_t)w * h * 4, (uint8_t *)px);
	xcb_flush(c);
	printf("ready %u\n", win);
	fflush(stdout);

	for (int t = 0; t < secs * 10 && !want_quit; t++) {
		if (want_raise) {
			want_raise = 0;
			uint32_t above = XCB_STACK_MODE_ABOVE;
			xcb_configure_window(c, win, XCB_CONFIG_WINDOW_STACK_MODE,
				&above);
		}
		if (want_toggle) {
			want_toggle = 0;
			shaped = !shaped;
			if (shaped && nrects)
				xcb_shape_rectangles(c, XCB_SHAPE_SO_SET,
					XCB_SHAPE_SK_BOUNDING,
					XCB_CLIP_ORDERING_UNSORTED, win, 0, 0,
					nrects, rects);
			else
				xcb_shape_mask(c, XCB_SHAPE_SO_SET,
					XCB_SHAPE_SK_BOUNDING, win, 0, 0,
					XCB_NONE);
			/* what a program does on the Expose a shape change
			 * brings: paint it again */
			xcb_put_image(c, XCB_IMAGE_FORMAT_Z_PIXMAP, win, gc, w, h,
				0, 0, 0, 32, (uint32_t)w * h * 4, (uint8_t *)px);
		}
		xcb_flush(c);
		usleep(100000);
	}
	return 0;
}
