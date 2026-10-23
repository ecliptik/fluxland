/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * fuzz_command.c - Fuzz the command language and client pattern parsers
 *
 * Inputs reach these parsers from key/mouse binding files, menu
 * [command] items, the apps file and IPC, so they must survive anything.
 *
 * Run: ./fuzz_command tests/fuzz/corpus/command/
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "client_pattern.h"
#include "command.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	char *str = malloc(size + 1);
	if (!str)
		return 0;
	memcpy(str, data, size);
	str[size] = '\0';

	struct wm_command *cmd = wm_command_parse(str);
	if (cmd) {
		(void)wm_command_contains(cmd, WM_ACTION_EXEC);
		wm_command_destroy(cmd);
	}
	wm_condition_destroy(wm_condition_parse(str));
	wm_client_pattern_destroy(wm_client_pattern_parse(str, NULL));

	struct wm_window_list_spec spec;
	if (wm_window_list_parse(str, &spec))
		wm_window_list_finish(&spec);

	free(str);
	return 0;
}
