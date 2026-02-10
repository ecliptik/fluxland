/*
 * wm-wayland - A Fluxbox-inspired Wayland compositor
 * session_lock.c - ext-session-lock-v1 protocol support
 *
 * Implements the ext-session-lock-v1 protocol for screen lockers.
 * When a lock is active:
 *  - Lock surfaces cover each output in the overlay layer
 *  - Keyboard input is directed exclusively to the lock surface
 *  - All normal window interaction is blocked
 *  - Only VT switch keybindings are allowed
 */

#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdlib.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_session_lock_v1.h>
#include <wlr/util/log.h>

#include "session_lock.h"
#include "output.h"
#include "server.h"
#include "view.h"

/*
 * Focus the first mapped lock surface for keyboard input.
 */
static void
focus_lock_surface(struct wm_session_lock *lock)
{
	struct wm_lock_surface *ls;
	wl_list_for_each(ls, &lock->lock_surfaces, link) {
		if (ls->lock_surface->surface->mapped) {
			struct wlr_keyboard *kb =
				wlr_seat_get_keyboard(lock->server->seat);
			if (kb) {
				wlr_seat_keyboard_notify_enter(
					lock->server->seat,
					ls->lock_surface->surface,
					kb->keycodes, kb->num_keycodes,
					&kb->modifiers);
			} else {
				wlr_seat_keyboard_notify_enter(
					lock->server->seat,
					ls->lock_surface->surface,
					NULL, 0, NULL);
			}
			return;
		}
	}
}

/*
 * Configure a lock surface to fill its output.
 */
static void
configure_lock_surface(struct wm_lock_surface *ls)
{
	struct wlr_output *output = ls->lock_surface->output;
	if (!output)
		return;

	int width, height;
	wlr_output_effective_resolution(output, &width, &height);
	wlr_session_lock_surface_v1_configure(
		ls->lock_surface, (uint32_t)width, (uint32_t)height);
}

static void
handle_lock_surface_map(struct wl_listener *listener, void *data)
{
	struct wm_lock_surface *ls =
		wl_container_of(listener, ls, map);
	struct wm_server *server = ls->lock_surface->data;

	wlr_log(WLR_INFO, "lock surface mapped on output %s",
		ls->lock_surface->output ?
		ls->lock_surface->output->name : "(unknown)");

	focus_lock_surface(&server->session_lock);
}

static void
handle_lock_surface_destroy(struct wl_listener *listener, void *data)
{
	struct wm_lock_surface *ls =
		wl_container_of(listener, ls, destroy);

	wlr_log(WLR_INFO, "%s", "lock surface destroyed");

	wl_list_remove(&ls->map.link);
	wl_list_remove(&ls->destroy.link);
	wl_list_remove(&ls->surface_commit.link);
	wl_list_remove(&ls->output_mode.link);
	wl_list_remove(&ls->link);

	/* The scene tree is destroyed automatically by wlroots */
	free(ls);
}

static void
handle_lock_surface_commit(struct wl_listener *listener, void *data)
{
	struct wm_lock_surface *ls =
		wl_container_of(listener, ls, surface_commit);

	/* Reconfigure on initial commit */
	if (ls->lock_surface->surface->mapped)
		return;
}

static void
handle_lock_surface_output_mode(struct wl_listener *listener, void *data)
{
	struct wm_lock_surface *ls =
		wl_container_of(listener, ls, output_mode);

	/* Output resolution changed, reconfigure lock surface */
	configure_lock_surface(ls);
}

/*
 * Handle a new lock surface being created (one per output).
 */
static void
handle_new_lock_surface(struct wl_listener *listener, void *data)
{
	struct wm_session_lock *lock =
		wl_container_of(listener, lock, lock_new_surface);
	struct wlr_session_lock_surface_v1 *lock_surface = data;

	struct wm_lock_surface *ls = calloc(1, sizeof(*ls));
	if (!ls) {
		wlr_log(WLR_ERROR, "%s", "failed to allocate lock surface");
		return;
	}

	ls->lock_surface = lock_surface;

	/* Store server reference for use in callbacks */
	lock_surface->data = lock->server;

	/* Create scene tree in the overlay layer */
	ls->scene_tree = wlr_scene_subsurface_tree_create(
		lock->server->layer_overlay, lock_surface->surface);
	if (!ls->scene_tree) {
		wlr_log(WLR_ERROR, "%s", "failed to create lock surface scene tree");
		free(ls);
		return;
	}

	/* Position the lock surface at the output's layout position */
	struct wlr_output *output = lock_surface->output;
	if (output) {
		struct wlr_box output_box;
		wlr_output_layout_get_box(
			lock->server->output_layout, output, &output_box);
		wlr_scene_node_set_position(
			&ls->scene_tree->node, output_box.x, output_box.y);
	}

	/* Set up listeners */
	ls->map.notify = handle_lock_surface_map;
	wl_signal_add(&lock_surface->surface->events.map, &ls->map);

	ls->destroy.notify = handle_lock_surface_destroy;
	wl_signal_add(&lock_surface->events.destroy, &ls->destroy);

	ls->surface_commit.notify = handle_lock_surface_commit;
	wl_signal_add(&lock_surface->surface->events.commit,
		&ls->surface_commit);

	ls->output_mode.notify = handle_lock_surface_output_mode;
	if (output) {
		wl_signal_add(&output->events.commit, &ls->output_mode);
	} else {
		wl_list_init(&ls->output_mode.link);
	}

	wl_list_insert(&lock->lock_surfaces, &ls->link);

	/* Send initial configure with the output's dimensions */
	configure_lock_surface(ls);

	wlr_log(WLR_INFO, "new lock surface on output %s",
		output ? output->name : "(unknown)");
}

/*
 * Handle the lock client requesting unlock.
 */
static void
handle_lock_unlock(struct wl_listener *listener, void *data)
{
	struct wm_session_lock *lock =
		wl_container_of(listener, lock, lock_unlock);

	wlr_log(WLR_INFO, "session unlocked");

	lock->locked = false;
	lock->active_lock = NULL;

	/* Remove per-lock listeners */
	wl_list_remove(&lock->lock_new_surface.link);
	wl_list_remove(&lock->lock_unlock.link);
	wl_list_remove(&lock->lock_destroy.link);
	wl_list_init(&lock->lock_new_surface.link);
	wl_list_init(&lock->lock_unlock.link);
	wl_list_init(&lock->lock_destroy.link);

	/* Restore keyboard focus to the previously focused view */
	if (lock->server->focused_view) {
		struct wlr_surface *surface =
			lock->server->focused_view->xdg_toplevel->base->surface;
		struct wlr_keyboard *kb =
			wlr_seat_get_keyboard(lock->server->seat);
		if (kb) {
			wlr_seat_keyboard_notify_enter(
				lock->server->seat, surface,
				kb->keycodes, kb->num_keycodes,
				&kb->modifiers);
		}
	} else {
		wlr_seat_keyboard_clear_focus(lock->server->seat);
	}

	/* Clear pointer focus so it gets re-evaluated on next motion */
	wlr_seat_pointer_clear_focus(lock->server->seat);
}

/*
 * Handle the lock being destroyed (client disconnected).
 */
static void
handle_lock_destroy(struct wl_listener *listener, void *data)
{
	struct wm_session_lock *lock =
		wl_container_of(listener, lock, lock_destroy);

	/*
	 * If the lock client crashes while we're locked, we stay locked
	 * (security measure). The user must VT switch and kill the compositor
	 * or start a new lock client.
	 */
	if (lock->locked) {
		wlr_log(WLR_ERROR,
			"lock client destroyed while session is locked! "
			"Session remains locked for security.");
	}

	lock->active_lock = NULL;

	wl_list_remove(&lock->lock_new_surface.link);
	wl_list_remove(&lock->lock_unlock.link);
	wl_list_remove(&lock->lock_destroy.link);
	wl_list_init(&lock->lock_new_surface.link);
	wl_list_init(&lock->lock_unlock.link);
	wl_list_init(&lock->lock_destroy.link);
}

/*
 * Handle a new lock request from a client.
 */
static void
handle_new_lock(struct wl_listener *listener, void *data)
{
	struct wm_session_lock *lock =
		wl_container_of(listener, lock, new_lock);
	struct wlr_session_lock_v1 *wlr_lock = data;

	if (lock->active_lock) {
		wlr_log(WLR_INFO,
			"rejecting new lock: session already locked");
		wlr_session_lock_v1_destroy(wlr_lock);
		return;
	}

	wlr_log(WLR_INFO, "new session lock requested");

	lock->active_lock = wlr_lock;
	lock->locked = true;

	/* Set up per-lock listeners */
	lock->lock_new_surface.notify = handle_new_lock_surface;
	wl_signal_add(&wlr_lock->events.new_surface,
		&lock->lock_new_surface);

	lock->lock_unlock.notify = handle_lock_unlock;
	wl_signal_add(&wlr_lock->events.unlock, &lock->lock_unlock);

	lock->lock_destroy.notify = handle_lock_destroy;
	wl_signal_add(&wlr_lock->events.destroy, &lock->lock_destroy);

	/* Tell the lock client we're locked — it can now create surfaces */
	wlr_session_lock_v1_send_locked(wlr_lock);

	/* Clear keyboard focus from normal views */
	wlr_seat_keyboard_clear_focus(lock->server->seat);
	wlr_seat_pointer_clear_focus(lock->server->seat);
}

/*
 * Handle the session lock manager being destroyed.
 */
static void
handle_manager_destroy(struct wl_listener *listener, void *data)
{
	struct wm_session_lock *lock =
		wl_container_of(listener, lock, manager_destroy);

	wl_list_remove(&lock->new_lock.link);
	wl_list_remove(&lock->manager_destroy.link);
	wl_list_init(&lock->new_lock.link);
	wl_list_init(&lock->manager_destroy.link);

	lock->manager = NULL;
}

void
wm_session_lock_init(struct wm_server *server)
{
	struct wm_session_lock *lock = &server->session_lock;
	lock->server = server;
	lock->locked = false;
	lock->active_lock = NULL;

	wl_list_init(&lock->lock_surfaces);
	wl_list_init(&lock->lock_new_surface.link);
	wl_list_init(&lock->lock_unlock.link);
	wl_list_init(&lock->lock_destroy.link);

	lock->manager = wlr_session_lock_manager_v1_create(
		server->wl_display);
	if (!lock->manager) {
		wlr_log(WLR_ERROR, "failed to create session lock manager");
		return;
	}

	lock->new_lock.notify = handle_new_lock;
	wl_signal_add(&lock->manager->events.new_lock, &lock->new_lock);

	lock->manager_destroy.notify = handle_manager_destroy;
	wl_signal_add(&lock->manager->events.destroy,
		&lock->manager_destroy);

	wlr_log(WLR_INFO, "session lock protocol initialized");
}

void
wm_session_lock_finish(struct wm_server *server)
{
	struct wm_session_lock *lock = &server->session_lock;

	wl_list_remove(&lock->new_lock.link);
	wl_list_remove(&lock->manager_destroy.link);
	wl_list_remove(&lock->lock_new_surface.link);
	wl_list_remove(&lock->lock_unlock.link);
	wl_list_remove(&lock->lock_destroy.link);
}

bool
wm_session_is_locked(struct wm_server *server)
{
	return server->session_lock.locked;
}
