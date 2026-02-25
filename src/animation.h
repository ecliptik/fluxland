/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * animation.h - Window animation infrastructure (fade in/out)
 */

#ifndef WM_ANIMATION_H
#define WM_ANIMATION_H

#include <stdbool.h>
#include <wayland-server-core.h>

struct wm_view;

enum wm_animation_type {
	WM_ANIM_FADE_IN,    /* opacity 0 -> target */
	WM_ANIM_FADE_OUT,   /* current opacity -> 0 */
};

/* Callback invoked when an animation finishes (completed or cancelled). */
typedef void (*wm_animation_done_fn)(struct wm_view *view, bool completed,
	void *data);

struct wm_animation {
	struct wm_view *view;
	struct wl_event_source *timer;
	enum wm_animation_type type;

	/* Opacity range (0-255) */
	int start_alpha;
	int target_alpha;

	/* Timing */
	int duration_ms;
	int elapsed_ms;

	/* Completion callback */
	wm_animation_done_fn done_fn;
	void *done_data;
};

/*
 * Start a fade animation on a view.
 * If the view already has an animation, it is cancelled first.
 * Returns the animation, or NULL on failure.
 */
struct wm_animation *wm_animation_start(struct wm_view *view,
	enum wm_animation_type type, int start_alpha, int target_alpha,
	int duration_ms, wm_animation_done_fn done_fn, void *done_data);

/*
 * Cancel an in-progress animation. Calls done_fn with completed=false.
 * Safe to call with NULL.
 */
void wm_animation_cancel(struct wm_animation *anim);

#endif /* WM_ANIMATION_H */
