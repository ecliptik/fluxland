/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * perf.c - Compile-time performance profiling infrastructure
 */

#ifdef WM_PERF_ENABLE

#include "perf.h"
#include <string.h>
#include <wlr/util/log.h>

void
wm_perf_probe_init(struct wm_perf_probe *probe, const char *label)
{
	memset(probe, 0, sizeof(*probe));
	probe->label = label;
	probe->min_ns = UINT64_MAX;
}

void
wm_perf_probe_record(struct wm_perf_probe *probe, uint64_t duration_ns)
{
	probe->samples[probe->head].duration_ns = duration_ns;
	probe->head = (probe->head + 1) % WM_PERF_RING_SIZE;
	if (probe->count < WM_PERF_RING_SIZE)
		probe->count++;

	if (duration_ns < probe->min_ns)
		probe->min_ns = duration_ns;
	if (duration_ns > probe->max_ns)
		probe->max_ns = duration_ns;
	probe->sum_ns += duration_ns;
	probe->total_count++;
}

void
wm_perf_probe_report(struct wm_perf_probe *probe)
{
	if (probe->total_count == 0) {
		wlr_log(WLR_INFO, "perf [%s]: no samples", probe->label);
		return;
	}

	uint64_t avg_ns = probe->sum_ns / probe->total_count;
	wlr_log(WLR_INFO, "perf [%s]: count=%lu min=%.3fms max=%.3fms avg=%.3fms",
		probe->label,
		(unsigned long)probe->total_count,
		(double)probe->min_ns / 1e6,
		(double)probe->max_ns / 1e6,
		(double)avg_ns / 1e6);
}

void
wm_perf_probe_reset(struct wm_perf_probe *probe)
{
	const char *label = probe->label;
	memset(probe, 0, sizeof(*probe));
	probe->label = label;
	probe->min_ns = UINT64_MAX;
}

#endif /* WM_PERF_ENABLE */
