/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * perf.h - Compile-time performance profiling infrastructure
 *
 * Zero-cost no-ops when WM_PERF_ENABLE is not defined.
 * Enable with meson option -Dperf=true.
 */

#ifndef WM_PERF_H
#define WM_PERF_H

#include <stdint.h>
#include <time.h>

#define WM_PERF_RING_SIZE 256

struct wm_perf_sample {
	uint64_t duration_ns;
};

struct wm_perf_probe {
	const char *label;
	struct wm_perf_sample samples[WM_PERF_RING_SIZE];
	int head;
	int count;
	uint64_t min_ns;
	uint64_t max_ns;
	uint64_t sum_ns;
	uint64_t total_count;
};

#ifdef WM_PERF_ENABLE

/* Initialize a probe with a label */
void wm_perf_probe_init(struct wm_perf_probe *probe, const char *label);

/* Record a duration sample */
void wm_perf_probe_record(struct wm_perf_probe *probe, uint64_t duration_ns);

/* Log a report to wlr_log */
void wm_perf_probe_report(struct wm_perf_probe *probe);

/* Reset all stats */
void wm_perf_probe_reset(struct wm_perf_probe *probe);

/* Timing macros */
#define WM_PERF_BEGIN(sample) \
	struct timespec _perf_ts_begin_##sample; \
	clock_gettime(CLOCK_MONOTONIC, &_perf_ts_begin_##sample)

#define WM_PERF_END(sample, probe) do { \
	struct timespec _perf_ts_end_##sample; \
	clock_gettime(CLOCK_MONOTONIC, &_perf_ts_end_##sample); \
	uint64_t _ns = (uint64_t)(_perf_ts_end_##sample.tv_sec - \
		_perf_ts_begin_##sample.tv_sec) * 1000000000ULL + \
		(uint64_t)(_perf_ts_end_##sample.tv_nsec - \
		_perf_ts_begin_##sample.tv_nsec); \
	wm_perf_probe_record((probe), _ns); \
} while (0)

#else /* !WM_PERF_ENABLE */

#define WM_PERF_BEGIN(sample) ((void)0)
#define WM_PERF_END(sample, probe) ((void)0)

static inline void wm_perf_probe_init(struct wm_perf_probe *probe, const char *label) {
	(void)probe; (void)label;
}
static inline void wm_perf_probe_record(struct wm_perf_probe *probe, uint64_t ns) {
	(void)probe; (void)ns;
}
static inline void wm_perf_probe_report(struct wm_perf_probe *probe) {
	(void)probe;
}
static inline void wm_perf_probe_reset(struct wm_perf_probe *probe) {
	(void)probe;
}

#endif /* WM_PERF_ENABLE */

#endif /* WM_PERF_H */
