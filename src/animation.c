/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * animation.c - Window animation infrastructure (fade in/out)
 *
 * Uses wl_event_loop timer to drive ~60fps opacity interpolation.
 */

#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <wlr/util/log.h>

#include "animation.h"
#include "view.h"

#define ANIM_FRAME_MS 16 /* ~60fps */

static int
animation_timer_callback(void *data)
{
	struct wm_animation *anim = data;

	/*
	 * Safety: view may have been destroyed if cancel wasn't called.
	 * This shouldn't happen because destroy handler cancels, but
	 * guard defensively.
	 */
	if (!anim->view) {
		free(anim);
		return 0;
	}

	anim->elapsed_ms += ANIM_FRAME_MS;

	if (anim->elapsed_ms >= anim->duration_ms) {
		/* Animation complete: snap to target */
		wm_view_set_opacity(anim->view, anim->target_alpha);

		struct wm_view *view = anim->view;
		wm_animation_done_fn done_fn = anim->done_fn;
		void *done_data = anim->done_data;

		/* Clear the view's animation pointer before callback */
		view->animation = NULL;

		wl_event_source_remove(anim->timer);
		free(anim);

		if (done_fn)
			done_fn(view, true, done_data);
		return 0;
	}

	/* Linear interpolation */
	float t = (float)anim->elapsed_ms / (float)anim->duration_ms;
	int alpha = anim->start_alpha +
		(int)((float)(anim->target_alpha - anim->start_alpha) * t);
	wm_view_set_opacity(anim->view, alpha);

	/* Schedule next frame */
	wl_event_source_timer_update(anim->timer, ANIM_FRAME_MS);
	return 0;
}

struct wm_animation *
wm_animation_start(struct wm_view *view, enum wm_animation_type type,
	int start_alpha, int target_alpha, int duration_ms,
	wm_animation_done_fn done_fn, void *done_data)
{
	if (!view || !view->server)
		return NULL;

	/* Cancel any existing animation on this view */
	if (view->animation)
		wm_animation_cancel(view->animation);

	/* Skip animation if duration is too short */
	if (duration_ms <= 0) {
		wm_view_set_opacity(view, target_alpha);
		if (done_fn)
			done_fn(view, true, done_data);
		return NULL;
	}

	struct wm_animation *anim = calloc(1, sizeof(*anim));
	if (!anim) {
		wm_view_set_opacity(view, target_alpha);
		if (done_fn)
			done_fn(view, true, done_data);
		return NULL;
	}

	anim->view = view;
	anim->type = type;
	anim->start_alpha = start_alpha;
	anim->target_alpha = target_alpha;
	anim->duration_ms = duration_ms;
	anim->elapsed_ms = 0;
	anim->done_fn = done_fn;
	anim->done_data = done_data;

	anim->timer = wl_event_loop_add_timer(view->server->wl_event_loop,
		animation_timer_callback, anim);
	if (!anim->timer) {
		wlr_log(WLR_ERROR, "failed to create animation timer");
		free(anim);
		wm_view_set_opacity(view, target_alpha);
		if (done_fn)
			done_fn(view, true, done_data);
		return NULL;
	}

	/* Set initial opacity and fire first frame */
	wm_view_set_opacity(view, start_alpha);
	view->animation = anim;
	wl_event_source_timer_update(anim->timer, ANIM_FRAME_MS);

	return anim;
}

void
wm_animation_cancel(struct wm_animation *anim)
{
	if (!anim)
		return;

	struct wm_view *view = anim->view;
	wm_animation_done_fn done_fn = anim->done_fn;
	void *done_data = anim->done_data;

	/* Clear view's animation pointer */
	if (view)
		view->animation = NULL;

	wl_event_source_remove(anim->timer);
	free(anim);

	if (done_fn)
		done_fn(view, false, done_data);
}
