/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * test_perf.c - Performance profiling infrastructure tests
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Force-enable perf for testing */
#define WM_PERF_ENABLE

/* Stub wlr_log */
#ifndef WLR_UTIL_LOG_H
#define WLR_UTIL_LOG_H
enum wlr_log_importance { WLR_SILENT, WLR_ERROR, WLR_INFO, WLR_DEBUG };
#define wlr_log(verb, fmt, ...) ((void)0)
#define _wlr_log(verb, fmt, ...) ((void)0)
#endif

#include "../src/perf.c"

static void
test_probe_init(void)
{
	struct wm_perf_probe probe;
	wm_perf_probe_init(&probe, "test");
	assert(strcmp(probe.label, "test") == 0);
	assert(probe.count == 0);
	assert(probe.total_count == 0);
	assert(probe.head == 0);
	assert(probe.min_ns == UINT64_MAX);
	assert(probe.max_ns == 0);
	assert(probe.sum_ns == 0);
	printf("  PASS: probe_init\n");
}

static void
test_probe_record(void)
{
	struct wm_perf_probe probe;
	wm_perf_probe_init(&probe, "test_record");

	wm_perf_probe_record(&probe, 1000);
	assert(probe.count == 1);
	assert(probe.total_count == 1);
	assert(probe.min_ns == 1000);
	assert(probe.max_ns == 1000);
	assert(probe.sum_ns == 1000);

	wm_perf_probe_record(&probe, 500);
	assert(probe.count == 2);
	assert(probe.total_count == 2);
	assert(probe.min_ns == 500);
	assert(probe.max_ns == 1000);
	assert(probe.sum_ns == 1500);

	wm_perf_probe_record(&probe, 2000);
	assert(probe.count == 3);
	assert(probe.total_count == 3);
	assert(probe.min_ns == 500);
	assert(probe.max_ns == 2000);
	assert(probe.sum_ns == 3500);

	printf("  PASS: probe_record\n");
}

static void
test_probe_ring_buffer_wrap(void)
{
	struct wm_perf_probe probe;
	wm_perf_probe_init(&probe, "ring");

	/* Fill ring buffer past capacity */
	for (int i = 0; i < WM_PERF_RING_SIZE + 10; i++) {
		wm_perf_probe_record(&probe, (uint64_t)(i + 1) * 100);
	}

	assert(probe.count == WM_PERF_RING_SIZE);
	assert(probe.total_count == WM_PERF_RING_SIZE + 10);
	assert(probe.head == 10); /* wrapped around */
	assert(probe.min_ns == 100);

	printf("  PASS: ring_buffer_wrap\n");
}

static void
test_probe_reset(void)
{
	struct wm_perf_probe probe;
	wm_perf_probe_init(&probe, "reset_test");

	wm_perf_probe_record(&probe, 1000);
	wm_perf_probe_record(&probe, 2000);

	wm_perf_probe_reset(&probe);
	assert(strcmp(probe.label, "reset_test") == 0);
	assert(probe.count == 0);
	assert(probe.total_count == 0);
	assert(probe.min_ns == UINT64_MAX);
	assert(probe.max_ns == 0);
	assert(probe.sum_ns == 0);

	printf("  PASS: probe_reset\n");
}

static void
test_probe_report_no_samples(void)
{
	struct wm_perf_probe probe;
	wm_perf_probe_init(&probe, "empty");
	/* Should not crash */
	wm_perf_probe_report(&probe);
	printf("  PASS: report_no_samples\n");
}

static void
test_timing_macros(void)
{
	struct wm_perf_probe probe;
	wm_perf_probe_init(&probe, "macros");

	WM_PERF_BEGIN(test);
	/* Do some minimal work */
	volatile int x = 0;
	for (int i = 0; i < 100; i++)
		x += i;
	(void)x;
	WM_PERF_END(test, &probe);

	assert(probe.total_count == 1);
	assert(probe.min_ns > 0);
	printf("  PASS: timing_macros\n");
}

int
main(void)
{
	printf("test_perf:\n");
	test_probe_init();
	test_probe_record();
	test_probe_ring_buffer_wrap();
	test_probe_reset();
	test_probe_report_no_samples();
	test_timing_macros();
	printf("All perf tests passed.\n");
	return 0;
}
