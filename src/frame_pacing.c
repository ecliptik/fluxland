/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 *
 * frame_pacing.c - fifo-v1 and commit-timing-v1
 *
 * Both protocols add a readiness constraint to a content update (a
 * wl_surface.commit): fifo waits for a display refresh to have passed, and
 * commit timing for a time. A commit with a constraint is held with
 * wlr_surface_lock_pending(); wlroots then caches it and every commit after
 * it, so content updates are still applied in the order they were made.
 * The held commit is let go (wlr_surface_unlock_cached()) by the output's
 * frame handler, just before the frame is rendered, when its constraints
 * are met (wm_frame_pacing_output_frame()).
 *
 * The fifo barrier is set when the commit that asked for it is applied. It
 * is latched by the next frame of the output the surface is on (that
 * frame shows the commit) and cleared by the frame after that, which is
 * the first one allowed to show the next commit: every update a client
 * makes with set_barrier and wait_barrier is shown for at least one refresh.
 *
 * Constraints are not enforced where nothing would ever let them go: a
 * surface that is not on an output, a synchronized subsurface (the protocol
 * says to ignore them there), and an output that has stopped producing
 * frames (a watchdog lets the commits go, as the protocol allows).
 */

#define _POSIX_C_SOURCE 200809L

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#include <wayland-server-core.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/util/addon.h>
#include <wlr/util/log.h>

#include "commit-timing-v1-protocol.h"
#include "fifo-v1-protocol.h"
#include "frame_pacing.h"
#include "server.h"

#define NSEC_PER_SEC 1000000000LL
/* An output that has made no frame for this long is not pacing anything */
#define OUTPUT_STALL_NSEC (250LL * 1000 * 1000)
#define WATCHDOG_MSEC 100
/* A target time this close to the predicted presentation is met by it */
#define TIMESTAMP_TOLERANCE_DIVISOR 8

struct wm_frame_pacing {
	struct wm_server *server;
	struct wl_global *fifo_global;
	struct wl_global *commit_timing_global;
	struct wl_listener display_destroy;

	struct wl_list surfaces; /* pacing_surface.link */
	struct wl_list outputs; /* pacing_output.link */
	struct wl_event_source *watchdog;
};

/* What is known about an output's refresh cycle, from its frame and present events */
struct pacing_output {
	struct wl_list link; /* wm_frame_pacing.outputs */
	struct wm_frame_pacing *fp;
	struct wlr_output *output;
	struct wl_listener present;
	struct wl_listener destroy;
	int64_t last_present_nsec; /* 0: none yet */
	int64_t refresh_nsec;
	int64_t last_frame_nsec;
};

/* A commit that carries a constraint, or sets a barrier */
struct pacing_commit {
	struct wl_list link; /* pacing_surface.commits, in commit order */
	uint32_t seq; /* wlr_surface_state.seq of the commit */
	bool set_barrier;
	bool wait_barrier;
	bool has_timestamp;
	int64_t timestamp_nsec;
	bool held; /* wlr_surface_lock_pending() is in effect */
};

struct pacing_surface {
	struct wm_frame_pacing *fp;
	struct wlr_surface *surface;
	struct wlr_addon addon;
	struct wl_list link; /* wm_frame_pacing.surfaces */
	struct wl_resource *fifo;
	struct wl_resource *timer;

	/* Double-buffered state the objects have set since the last commit */
	bool pending_set_barrier;
	bool pending_wait_barrier;
	bool pending_has_timestamp;
	int64_t pending_timestamp_nsec;

	bool barrier_set;
	/* A frame has shown the content update that set the barrier */
	bool barrier_latched;

	struct wl_list commits;
	struct wl_listener client_commit;
	struct wl_listener commit;
};

static int64_t
timespec_nsec(const struct timespec *ts)
{
	return (int64_t)ts->tv_sec * NSEC_PER_SEC + ts->tv_nsec;
}

static int64_t
now_nsec(struct wm_frame_pacing *fp)
{
	struct timespec ts;
	/* The presentation clock wlroots reports to clients (wp_presentation) */
	(void)fp;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return timespec_nsec(&ts);
}

/* --- Outputs --- */

static void
pacing_output_destroy(struct pacing_output *po)
{
	wl_list_remove(&po->present.link);
	wl_list_remove(&po->destroy.link);
	wl_list_remove(&po->link);
	free(po);
}

static void
handle_output_present(struct wl_listener *listener, void *data)
{
	struct pacing_output *po = wl_container_of(listener, po, present);
	const struct wlr_output_event_present *event = data;
	if (!event->presented)
		return;
	po->last_present_nsec = timespec_nsec(&event->when);
	po->refresh_nsec = event->refresh;
}

static void
handle_output_destroy(struct wl_listener *listener, void *data)
{
	(void)data;
	struct pacing_output *po = wl_container_of(listener, po, destroy);
	pacing_output_destroy(po);
}

static struct pacing_output *
pacing_output_find(struct wm_frame_pacing *fp, struct wlr_output *output)
{
	struct pacing_output *po;
	wl_list_for_each(po, &fp->outputs, link) {
		if (po->output == output)
			return po;
	}
	return NULL;
}

static struct pacing_output *
pacing_output_get(struct wm_frame_pacing *fp, struct wlr_output *output)
{
	struct pacing_output *po = pacing_output_find(fp, output);
	if (po)
		return po;
	po = calloc(1, sizeof(*po));
	if (!po)
		return NULL;
	po->fp = fp;
	po->output = output;
	po->last_frame_nsec = now_nsec(fp);
	po->present.notify = handle_output_present;
	wl_signal_add(&output->events.present, &po->present);
	po->destroy.notify = handle_output_destroy;
	wl_signal_add(&output->events.destroy, &po->destroy);
	wl_list_insert(&fp->outputs, &po->link);
	return po;
}

static int64_t
refresh_interval_nsec(const struct pacing_output *po)
{
	if (po->refresh_nsec > 0)
		return po->refresh_nsec;
	if (po->output->refresh > 0)
		return NSEC_PER_SEC * 1000 / po->output->refresh;
	return NSEC_PER_SEC / 60;
}

/*
 * When the next frame of the output will be shown: the first refresh after
 * now, counted from the last one that was reported presented.
 */
static int64_t
predict_next_presentation(struct wm_frame_pacing *fp,
	const struct pacing_output *po)
{
	int64_t now = now_nsec(fp);
	int64_t interval = refresh_interval_nsec(po);
	if (po->last_present_nsec <= 0 || po->last_present_nsec > now)
		return now + interval;
	int64_t cycles = (now - po->last_present_nsec) / interval + 1;
	return po->last_present_nsec + cycles * interval;
}

/* --- Surfaces --- */

static struct wlr_output *
surface_output(struct wlr_surface *surface)
{
	struct wlr_surface_output *so;
	wl_list_for_each(so, &surface->current_outputs, link)
		return so->output;
	return NULL;
}

static bool
surface_is_synchronized_subsurface(struct wlr_surface *surface)
{
	struct wlr_subsurface *sub =
		wlr_subsurface_try_from_wlr_surface(surface);
	return sub && sub->synchronized;
}

static bool
has_held_commits(const struct pacing_surface *ps)
{
	const struct pacing_commit *pc;
	wl_list_for_each(pc, &ps->commits, link) {
		if (pc->held)
			return true;
	}
	return false;
}

static void arm_watchdog(struct wm_frame_pacing *fp);

/*
 * Let go the held commits whose constraints are met, in order, stopping at
 * the first that is not (a later one can't be applied before it). With
 * ignore_constraints, all of them.
 */
static void
surface_release_commits(struct pacing_surface *ps, int64_t presentation_nsec,
	bool ignore_constraints)
{
	for (;;) {
		struct pacing_commit *pc = NULL, *it;
		wl_list_for_each(it, &ps->commits, link) {
			if (it->held) {
				pc = it;
				break;
			}
		}
		if (!pc)
			return;
		if (!ignore_constraints) {
			if (pc->wait_barrier && ps->barrier_set)
				return;
			int64_t tolerance = 0;
			struct wlr_output *output = surface_output(ps->surface);
			struct pacing_output *po = output ?
				pacing_output_find(ps->fp, output) : NULL;
			if (po)
				tolerance = refresh_interval_nsec(po) /
					TIMESTAMP_TOLERANCE_DIVISOR;
			if (pc->has_timestamp &&
					pc->timestamp_nsec > presentation_nsec + tolerance)
				return;
		}
		/*
		 * Applying the commit runs the commit handler, which may free
		 * entries (this one among them): nothing of pc is used after.
		 */
		uint32_t seq = pc->seq;
		pc->held = false;
		wlr_surface_unlock_cached(ps->surface, seq);
	}
}

/* A content update has been applied: the barriers it set are now set */
static void
handle_surface_commit(struct wl_listener *listener, void *data)
{
	(void)data;
	struct pacing_surface *ps = wl_container_of(listener, ps, commit);
	uint32_t applied = ps->surface->current.seq;
	struct pacing_commit *pc, *tmp;
	wl_list_for_each_safe(pc, tmp, &ps->commits, link) {
		if ((int32_t)(pc->seq - applied) > 0)
			continue;
		if (pc->set_barrier) {
			ps->barrier_set = true;
			ps->barrier_latched = false;
		}
		wl_list_remove(&pc->link);
		free(pc);
	}
}

/* wl_surface.commit: take the constraints the client has set for it */
static void
handle_surface_client_commit(struct wl_listener *listener, void *data)
{
	(void)data;
	struct pacing_surface *ps = wl_container_of(listener, ps, client_commit);
	if (!ps->pending_set_barrier && !ps->pending_wait_barrier &&
			!ps->pending_has_timestamp)
		return;

	struct pacing_commit *pc = calloc(1, sizeof(*pc));
	if (pc) {
		pc->seq = ps->surface->pending.seq;
		pc->set_barrier = ps->pending_set_barrier;
		pc->wait_barrier = ps->pending_wait_barrier;
		pc->has_timestamp = ps->pending_has_timestamp;
		pc->timestamp_nsec = ps->pending_timestamp_nsec;
		wl_list_insert(ps->commits.prev, &pc->link);
	}
	ps->pending_set_barrier = false;
	ps->pending_wait_barrier = false;
	ps->pending_has_timestamp = false;
	if (!pc)
		return;

	struct wlr_output *output = surface_output(ps->surface);
	if (!(pc->wait_barrier || pc->has_timestamp) || !output ||
			surface_is_synchronized_subsurface(ps->surface))
		return;
	struct pacing_output *po = pacing_output_get(ps->fp, output);
	if (!po)
		return;

	/* Held, then let go at once if nothing is in its way */
	wlr_surface_lock_pending(ps->surface);
	pc->held = true;
	surface_release_commits(ps, predict_next_presentation(ps->fp, po),
		false);
	if (has_held_commits(ps)) {
		/* Something has to run the output's frame handler */
		wlr_output_schedule_frame(output);
		arm_watchdog(ps->fp);
	}
}

static void
surface_unlink_resources(struct pacing_surface *ps)
{
	if (ps->fifo)
		wl_resource_set_user_data(ps->fifo, NULL);
	if (ps->timer)
		wl_resource_set_user_data(ps->timer, NULL);
	ps->fifo = NULL;
	ps->timer = NULL;
}

static void
surface_addon_destroy(struct wlr_addon *addon)
{
	struct pacing_surface *ps = wl_container_of(addon, ps, addon);
	struct pacing_commit *pc, *tmp;
	wl_list_for_each_safe(pc, tmp, &ps->commits, link) {
		wl_list_remove(&pc->link);
		free(pc);
	}
	surface_unlink_resources(ps);
	wl_list_remove(&ps->client_commit.link);
	wl_list_remove(&ps->commit.link);
	wl_list_remove(&ps->link);
	wlr_addon_finish(&ps->addon);
	free(ps);
}

static const struct wlr_addon_interface surface_addon_impl = {
	.name = "wm_frame_pacing_surface",
	.destroy = surface_addon_destroy,
};

static struct pacing_surface *
pacing_surface_get(struct wm_frame_pacing *fp, struct wlr_surface *surface)
{
	struct wlr_addon *addon =
		wlr_addon_find(&surface->addons, fp, &surface_addon_impl);
	if (addon) {
		struct pacing_surface *ps = wl_container_of(addon, ps, addon);
		return ps;
	}
	struct pacing_surface *ps = calloc(1, sizeof(*ps));
	if (!ps)
		return NULL;
	ps->fp = fp;
	ps->surface = surface;
	wl_list_init(&ps->commits);
	wlr_addon_init(&ps->addon, &surface->addons, fp, &surface_addon_impl);
	ps->client_commit.notify = handle_surface_client_commit;
	wl_signal_add(&surface->events.client_commit, &ps->client_commit);
	ps->commit.notify = handle_surface_commit;
	wl_signal_add(&surface->events.commit, &ps->commit);
	wl_list_insert(&fp->surfaces, &ps->link);
	return ps;
}

/* --- The frame of an output --- */

void
wm_frame_pacing_output_frame(struct wm_server *server,
	struct wlr_output *output)
{
	struct wm_frame_pacing *fp = server->frame_pacing;
	if (!fp)
		return;
	struct pacing_output *po = pacing_output_get(fp, output);
	if (!po)
		return;
	po->last_frame_nsec = now_nsec(fp);
	int64_t presentation = predict_next_presentation(fp, po);

	bool more = false;
	struct pacing_surface *ps, *tmp;
	wl_list_for_each_safe(ps, tmp, &fp->surfaces, link) {
		if (surface_output(ps->surface) != output)
			continue;
		/* The frame after the one that showed the update clears it */
		if (ps->barrier_set && ps->barrier_latched)
			ps->barrier_set = false;
		surface_release_commits(ps, presentation, false);
		/* This frame shows what was just applied */
		if (ps->barrier_set && !ps->barrier_latched)
			ps->barrier_latched = true;
		if (has_held_commits(ps))
			more = true;
	}
	if (more)
		wlr_output_schedule_frame(output);
}

/*
 * Commits are not held where nothing lets them go. A surface whose output
 * stopped making frames (or that left every output) has its constraints
 * dropped, so a client that waits on them is never stuck.
 */
static int
handle_watchdog(void *data)
{
	struct wm_frame_pacing *fp = data;
	int64_t now = now_nsec(fp);
	bool again = false;
	struct pacing_surface *ps, *tmp;
	wl_list_for_each_safe(ps, tmp, &fp->surfaces, link) {
		if (!has_held_commits(ps))
			continue;
		struct wlr_output *output = surface_output(ps->surface);
		struct pacing_output *po = output ?
			pacing_output_find(fp, output) : NULL;
		if (!po || now - po->last_frame_nsec > OUTPUT_STALL_NSEC) {
			ps->barrier_set = false;
			surface_release_commits(ps, now, true);
		}
		if (has_held_commits(ps)) {
			if (output)
				wlr_output_schedule_frame(output);
			again = true;
		}
	}
	if (again)
		wl_event_source_timer_update(fp->watchdog, WATCHDOG_MSEC);
	return 0;
}

static void
arm_watchdog(struct wm_frame_pacing *fp)
{
	if (fp->watchdog)
		wl_event_source_timer_update(fp->watchdog, WATCHDOG_MSEC);
}

/* --- Protocol objects --- */

static void
destroy_request(struct wl_client *client, struct wl_resource *resource)
{
	(void)client;
	wl_resource_destroy(resource);
}

/* wp_fifo_v1 */

static void
fifo_handle_resource_destroy(struct wl_resource *resource)
{
	struct pacing_surface *ps = wl_resource_get_user_data(resource);
	if (ps)
		ps->fifo = NULL;
}

static bool
fifo_check_surface(struct wl_resource *resource, struct pacing_surface **ps)
{
	*ps = wl_resource_get_user_data(resource);
	if (*ps)
		return true;
	wl_resource_post_error(resource, WP_FIFO_V1_ERROR_SURFACE_DESTROYED,
		"the surface of the fifo object is destroyed");
	return false;
}

static void
fifo_set_barrier(struct wl_client *client, struct wl_resource *resource)
{
	(void)client;
	struct pacing_surface *ps;
	if (fifo_check_surface(resource, &ps))
		ps->pending_set_barrier = true;
}

static void
fifo_wait_barrier(struct wl_client *client, struct wl_resource *resource)
{
	(void)client;
	struct pacing_surface *ps;
	if (fifo_check_surface(resource, &ps))
		ps->pending_wait_barrier = true;
}

static const struct wp_fifo_v1_interface fifo_impl = {
	.set_barrier = fifo_set_barrier,
	.wait_barrier = fifo_wait_barrier,
	.destroy = destroy_request,
};

static void
fifo_manager_get_fifo(struct wl_client *client,
	struct wl_resource *manager, uint32_t id,
	struct wl_resource *surface_resource)
{
	struct wm_frame_pacing *fp = wl_resource_get_user_data(manager);
	struct wlr_surface *surface = wlr_surface_from_resource(surface_resource);
	struct pacing_surface *ps = pacing_surface_get(fp, surface);
	if (!ps) {
		wl_client_post_no_memory(client);
		return;
	}
	if (ps->fifo) {
		wl_resource_post_error(manager, WP_FIFO_MANAGER_V1_ERROR_ALREADY_EXISTS,
			"a fifo object already exists for the surface");
		return;
	}
	struct wl_resource *resource = wl_resource_create(client,
		&wp_fifo_v1_interface, wl_resource_get_version(manager), id);
	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &fifo_impl, ps,
		fifo_handle_resource_destroy);
	ps->fifo = resource;
}

static const struct wp_fifo_manager_v1_interface fifo_manager_impl = {
	.destroy = destroy_request,
	.get_fifo = fifo_manager_get_fifo,
};

static void
bind_fifo_manager(struct wl_client *client, void *data, uint32_t version,
	uint32_t id)
{
	struct wl_resource *resource = wl_resource_create(client,
		&wp_fifo_manager_v1_interface, version, id);
	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &fifo_manager_impl, data,
		NULL);
}

/* wp_commit_timer_v1 */

static void
timer_handle_resource_destroy(struct wl_resource *resource)
{
	struct pacing_surface *ps = wl_resource_get_user_data(resource);
	if (ps)
		ps->timer = NULL;
}

static void
timer_set_timestamp(struct wl_client *client, struct wl_resource *resource,
	uint32_t tv_sec_hi, uint32_t tv_sec_lo, uint32_t tv_nsec)
{
	(void)client;
	struct pacing_surface *ps = wl_resource_get_user_data(resource);
	if (!ps) {
		wl_resource_post_error(resource,
			WP_COMMIT_TIMER_V1_ERROR_SURFACE_DESTROYED,
			"the surface of the commit timer is destroyed");
		return;
	}
	if (tv_nsec >= NSEC_PER_SEC) {
		wl_resource_post_error(resource,
			WP_COMMIT_TIMER_V1_ERROR_INVALID_TIMESTAMP,
			"tv_nsec is not below one second");
		return;
	}
	if (ps->pending_has_timestamp) {
		wl_resource_post_error(resource,
			WP_COMMIT_TIMER_V1_ERROR_TIMESTAMP_EXISTS,
			"a timestamp is already set for the next commit");
		return;
	}
	uint64_t sec = ((uint64_t)tv_sec_hi << 32) | tv_sec_lo;
	/* A time too far ahead to count in nanoseconds is a time never reached */
	if (sec > (uint64_t)(INT64_MAX / NSEC_PER_SEC) - 1)
		sec = (uint64_t)(INT64_MAX / NSEC_PER_SEC) - 1;
	ps->pending_has_timestamp = true;
	ps->pending_timestamp_nsec = (int64_t)sec * NSEC_PER_SEC + tv_nsec;
}

static const struct wp_commit_timer_v1_interface timer_impl = {
	.set_timestamp = timer_set_timestamp,
	.destroy = destroy_request,
};

static void
commit_timing_manager_get_timer(struct wl_client *client,
	struct wl_resource *manager, uint32_t id,
	struct wl_resource *surface_resource)
{
	struct wm_frame_pacing *fp = wl_resource_get_user_data(manager);
	struct wlr_surface *surface = wlr_surface_from_resource(surface_resource);
	struct pacing_surface *ps = pacing_surface_get(fp, surface);
	if (!ps) {
		wl_client_post_no_memory(client);
		return;
	}
	if (ps->timer) {
		wl_resource_post_error(manager,
			WP_COMMIT_TIMING_MANAGER_V1_ERROR_COMMIT_TIMER_EXISTS,
			"a commit timer already exists for the surface");
		return;
	}
	struct wl_resource *resource = wl_resource_create(client,
		&wp_commit_timer_v1_interface, wl_resource_get_version(manager),
		id);
	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &timer_impl, ps,
		timer_handle_resource_destroy);
	ps->timer = resource;
}

static const struct wp_commit_timing_manager_v1_interface
commit_timing_manager_impl = {
	.destroy = destroy_request,
	.get_timer = commit_timing_manager_get_timer,
};

static void
bind_commit_timing_manager(struct wl_client *client, void *data,
	uint32_t version, uint32_t id)
{
	struct wl_resource *resource = wl_resource_create(client,
		&wp_commit_timing_manager_v1_interface, version, id);
	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &commit_timing_manager_impl,
		data, NULL);
}

/* --- Setup --- */

static void
handle_display_destroy(struct wl_listener *listener, void *data)
{
	(void)data;
	struct wm_frame_pacing *fp =
		wl_container_of(listener, fp, display_destroy);
	struct pacing_output *po, *po_tmp;
	wl_list_for_each_safe(po, po_tmp, &fp->outputs, link)
		pacing_output_destroy(po);
	if (fp->watchdog)
		wl_event_source_remove(fp->watchdog);
	wl_list_remove(&fp->display_destroy.link);
	fp->server->frame_pacing = NULL;
	free(fp);
}

void
wm_frame_pacing_init(struct wm_server *server)
{
	struct wm_frame_pacing *fp = calloc(1, sizeof(*fp));
	if (!fp)
		return;
	fp->server = server;
	wl_list_init(&fp->surfaces);
	wl_list_init(&fp->outputs);
	fp->fifo_global = wl_global_create(server->wl_display,
		&wp_fifo_manager_v1_interface, 1, fp, bind_fifo_manager);
	fp->commit_timing_global = wl_global_create(server->wl_display,
		&wp_commit_timing_manager_v1_interface, 1, fp,
		bind_commit_timing_manager);
	struct wl_event_loop *loop =
		wl_display_get_event_loop(server->wl_display);
	fp->watchdog = wl_event_loop_add_timer(loop, handle_watchdog, fp);
	if (!fp->fifo_global || !fp->commit_timing_global || !fp->watchdog) {
		wlr_log(WLR_ERROR, "%s",
			"failed to create fifo-v1 / commit-timing-v1");
		if (fp->fifo_global)
			wl_global_destroy(fp->fifo_global);
		if (fp->commit_timing_global)
			wl_global_destroy(fp->commit_timing_global);
		if (fp->watchdog)
			wl_event_source_remove(fp->watchdog);
		free(fp);
		return;
	}
	fp->display_destroy.notify = handle_display_destroy;
	wl_display_add_destroy_listener(server->wl_display,
		&fp->display_destroy);
	server->frame_pacing = fp;
	wlr_log(WLR_INFO, "%s", "fifo-v1 and commit-timing-v1 initialized");
}
