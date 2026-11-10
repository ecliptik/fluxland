/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * test_tray_grant.c - Windows that may take the focus because of a tray click
 */

#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

#include "tray_grant.h"

static void
test_descendants(void)
{
	pid_t self = getpid();
	assert(wm_pid_is_descendant(self, self));
	assert(wm_pid_is_descendant(self, getppid()));	/* we are its child */
	assert(!wm_pid_is_descendant(getppid(), self));
	assert(!wm_pid_is_descendant(0, self));
	assert(!wm_pid_is_descendant(self, 0));
	assert(!wm_pid_is_descendant(-1, self));

	/* A child and its child */
	int up[2];
	assert(pipe(up) == 0);
	pid_t child = fork();
	if (child == 0) {
		pid_t grandchild = fork();
		if (grandchild == 0) {
			pause();
			_exit(0);
		}
		(void)!write(up[1], &grandchild, sizeof(grandchild));
		pause();
		_exit(0);
	}
	pid_t grandchild = 0;
	assert(read(up[0], &grandchild, sizeof(grandchild)) ==
		sizeof(grandchild));
	assert(wm_pid_is_descendant(child, self));
	assert(wm_pid_is_descendant(grandchild, self));
	assert(wm_pid_is_descendant(grandchild, child));
	assert(!wm_pid_is_descendant(child, grandchild));
	kill(grandchild, SIGKILL);
	kill(child, SIGKILL);
	waitpid(child, NULL, 0);
	printf("  PASS: descendants\n");
}

static void
test_grant_lifetime(void)
{
	wm_tray_grant_clear();
	int64_t t = 1000000;
	pid_t pid = getpid();

	/* Off (no grace): nothing is granted */
	assert(wm_tray_grant_begin(pid, t, 0) == 0);
	assert(wm_tray_grant_begin(0, t, 500) == 0);
	assert(!wm_tray_grant_allows(pid, t));

	/* Unanswered: allowed, up to the longest a click may wait */
	uint64_t id = wm_tray_grant_begin(pid, t, 500);
	assert(id != 0);
	assert(wm_tray_grant_allows(pid, t));
	assert(wm_tray_grant_allows(pid, t + WM_TRAY_GRANT_PENDING_MAX_MS - 1));
	assert(!wm_tray_grant_allows(pid, t + WM_TRAY_GRANT_PENDING_MAX_MS));

	/* Answered: the grace after the reply, from then */
	wm_tray_grant_end(id, t + 100);
	assert(wm_tray_grant_allows(pid, t + 599));
	assert(!wm_tray_grant_allows(pid, t + 600));

	/* Another program's windows were not asked about */
	wm_tray_grant_clear();
	id = wm_tray_grant_begin(pid, t, 500);
	assert(!wm_tray_grant_allows(1, t));	/* init is nobody's child */
	assert(!wm_tray_grant_allows(0, t));
	assert(!wm_tray_grant_allows(-5, t));

	/* An unknown or already used id changes nothing */
	wm_tray_grant_end(0, t);
	wm_tray_grant_end(id + 99, t);
	assert(wm_tray_grant_allows(pid, t + 4000));

	/* Slots are reused once grants run out; many clicks do not lose it */
	wm_tray_grant_clear();
	for (int i = 0; i < 40; i++) {
		id = wm_tray_grant_begin(pid, t + i, 500);
		assert(id != 0);
		wm_tray_grant_end(id, t + i);
	}
	assert(wm_tray_grant_allows(pid, t + 40));
	assert(!wm_tray_grant_allows(pid, t + 5000));
	wm_tray_grant_clear();
	assert(!wm_tray_grant_allows(pid, t));
	printf("  PASS: grant_lifetime\n");
}

static void
test_clock_moves_forward(void)
{
	int64_t a = wm_tray_grant_now_ms();
	assert(a > 0);
	assert(wm_tray_grant_now_ms() >= a);
	printf("  PASS: clock_moves_forward\n");
}

int
main(void)
{
	printf("test_tray_grant:\n");
	test_descendants();
	test_grant_lifetime();
	test_clock_moves_forward();
	printf("All tray_grant tests passed.\n");
	return 0;
}
