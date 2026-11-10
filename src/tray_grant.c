/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 *
 * tray_grant.c - Windows that may take the focus because of a tray click
 */

#define _GNU_SOURCE
#include "tray_grant.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_GRANTS 16
#define MAX_ANCESTORS 64

struct grant {
	uint64_t id;		/* 0: unused */
	pid_t pid;
	int64_t until_ms;
	int grace_ms;
	bool pending;		/* not answered yet */
};

static struct grant grants[MAX_GRANTS];
static uint64_t next_id = 1;

int64_t
wm_tray_grant_now_ms(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/* The parent of pid: from /proc/<pid>/stat, whose comm may hold anything */
static pid_t
parent_of(pid_t pid)
{
	char path[64];
	snprintf(path, sizeof(path), "/proc/%d/stat", (int)pid);
	FILE *f = fopen(path, "r");
	if (!f)
		return 0;
	char buf[1024];
	size_t n = fread(buf, 1, sizeof(buf) - 1, f);
	fclose(f);
	buf[n] = '\0';
	char *close = strrchr(buf, ')');
	if (!close)
		return 0;
	char state;
	int ppid;
	if (sscanf(close + 1, " %c %d", &state, &ppid) != 2)
		return 0;
	return ppid;
}

bool
wm_pid_is_descendant(pid_t pid, pid_t ancestor)
{
	if (pid <= 0 || ancestor <= 0)
		return false;
	for (int depth = 0; pid > 1 && depth < MAX_ANCESTORS; depth++) {
		if (pid == ancestor)
			return true;
		pid = parent_of(pid);
	}
	return false;
}

uint64_t
wm_tray_grant_begin(pid_t pid, int64_t now_ms, int grace_ms)
{
	if (pid <= 0 || grace_ms <= 0)
		return 0;
	struct grant *slot = NULL;
	for (int i = 0; i < MAX_GRANTS; i++) {
		struct grant *g = &grants[i];
		if (g->id == 0 || g->until_ms <= now_ms) {
			slot = g;
			break;
		}
	}
	if (!slot)	/* all busy: the oldest gives way */
		slot = &grants[0];
	slot->id = next_id++;
	slot->pid = pid;
	slot->grace_ms = grace_ms;
	slot->pending = true;
	slot->until_ms = now_ms + WM_TRAY_GRANT_PENDING_MAX_MS;
	return slot->id;
}

void
wm_tray_grant_end(uint64_t id, int64_t now_ms)
{
	if (id == 0)
		return;
	for (int i = 0; i < MAX_GRANTS; i++) {
		struct grant *g = &grants[i];
		if (g->id == id) {
			g->pending = false;
			g->until_ms = now_ms + g->grace_ms;
			return;
		}
	}
}

bool
wm_tray_grant_allows(pid_t window_pid, int64_t now_ms)
{
	for (int i = 0; i < MAX_GRANTS; i++) {
		struct grant *g = &grants[i];
		if (g->id != 0 && g->until_ms > now_ms &&
		    wm_pid_is_descendant(window_pid, g->pid))
			return true;
	}
	return false;
}

void
wm_tray_grant_clear(void)
{
	for (int i = 0; i < MAX_GRANTS; i++)
		grants[i].id = 0;
}
