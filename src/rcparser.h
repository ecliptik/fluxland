/*
 * wm-wayland - A Fluxbox-inspired Wayland compositor
 *
 * rcparser.h - X resource database style configuration file parser
 *
 * Parses Fluxbox init file format:
 *   session.screen0.workspaces: 4
 *   session.screen0.focusModel: MouseFocus
 */

#ifndef WM_RCPARSER_H
#define WM_RCPARSER_H

#include <stdbool.h>

struct rc_entry {
	char *key;
	char *value;
};

struct rc_database {
	struct rc_entry *entries;
	int count;
	int capacity;
};

/* Create an empty resource database */
struct rc_database *rc_create(void);

/* Destroy a resource database and free all memory */
void rc_destroy(struct rc_database *db);

/* Load resources from a file, returns 0 on success, -1 on error */
int rc_load(struct rc_database *db, const char *path);

/* Look up a string value, returns NULL if not found */
const char *rc_get_string(struct rc_database *db, const char *key);

/* Look up an integer value, returns default_val if not found or invalid */
int rc_get_int(struct rc_database *db, const char *key, int default_val);

/* Look up a boolean value (True/true/1 vs False/false/0) */
bool rc_get_bool(struct rc_database *db, const char *key, bool default_val);

/* Set a key-value pair in the database (overwrites existing) */
void rc_set(struct rc_database *db, const char *key, const char *value);

#endif /* WM_RCPARSER_H */
