/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * ipc_json.h - Minimal JSON helpers for the IPC protocol
 */

#ifndef WM_IPC_JSON_H
#define WM_IPC_JSON_H

/*
 * Quote and escape a string as a JSON string literal ("..."), or the
 * literal null for NULL. Returns a malloc'd string, NULL on OOM.
 */
char *wm_ipc_json_quote(const char *s);

/*
 * Return the value of a top-level "key" in a JSON object: string values
 * are unescaped, other scalars are returned as text. Returns a malloc'd
 * string, or NULL if the key is missing or the value is malformed.
 */
char *wm_ipc_json_get_string(const char *json, const char *key);

#endif /* WM_IPC_JSON_H */
