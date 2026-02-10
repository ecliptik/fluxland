/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 *
 * rcparser.c - X resource database style configuration file parser
 */

#define _POSIX_C_SOURCE 200809L

#include "rcparser.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define RC_INITIAL_CAPACITY 32
#define RC_LINE_MAX 1024

static char *
strip(char *s)
{
	while (isspace((unsigned char)*s))
		s++;
	if (*s == '\0')
		return s;
	char *end = s + strlen(s) - 1;
	while (end > s && isspace((unsigned char)*end))
		end--;
	*(end + 1) = '\0';
	return s;
}

struct rc_database *
rc_create(void)
{
	struct rc_database *db = calloc(1, sizeof(*db));
	if (!db)
		return NULL;
	db->capacity = RC_INITIAL_CAPACITY;
	db->entries = calloc(db->capacity, sizeof(struct rc_entry));
	if (!db->entries) {
		free(db);
		return NULL;
	}
	return db;
}

void
rc_destroy(struct rc_database *db)
{
	if (!db)
		return;
	for (int i = 0; i < db->count; i++) {
		free(db->entries[i].key);
		free(db->entries[i].value);
	}
	free(db->entries);
	free(db);
}

static int
rc_find(struct rc_database *db, const char *key)
{
	for (int i = 0; i < db->count; i++) {
		if (strcmp(db->entries[i].key, key) == 0)
			return i;
	}
	return -1;
}

void
rc_set(struct rc_database *db, const char *key, const char *value)
{
	int idx = rc_find(db, key);
	if (idx >= 0) {
		char *new_val = strdup(value);
		if (!new_val)
			return;
		free(db->entries[idx].value);
		db->entries[idx].value = new_val;
		return;
	}

	if (db->count >= db->capacity) {
		int new_cap = db->capacity * 2;
		struct rc_entry *new_entries =
			realloc(db->entries, new_cap * sizeof(struct rc_entry));
		if (!new_entries)
			return;
		db->entries = new_entries;
		db->capacity = new_cap;
	}

	db->entries[db->count].key = strdup(key);
	db->entries[db->count].value = strdup(value);
	if (!db->entries[db->count].key || !db->entries[db->count].value) {
		free(db->entries[db->count].key);
		free(db->entries[db->count].value);
		return;
	}
	db->count++;
}

int
rc_load(struct rc_database *db, const char *path)
{
	FILE *f = fopen(path, "r");
	if (!f)
		return -1;

	char line[RC_LINE_MAX];
	while (fgets(line, sizeof(line), f)) {
		char *p = strip(line);

		/* Skip empty lines and comments (# or !) */
		if (*p == '\0' || *p == '#' || *p == '!')
			continue;

		/* Find the first colon separator */
		char *colon = strchr(p, ':');
		if (!colon)
			continue;

		*colon = '\0';
		char *key = strip(p);
		char *value = strip(colon + 1);

		if (*key == '\0')
			continue;

		rc_set(db, key, value);
	}

	fclose(f);
	return 0;
}

const char *
rc_get_string(struct rc_database *db, const char *key)
{
	int idx = rc_find(db, key);
	if (idx < 0)
		return NULL;
	return db->entries[idx].value;
}

int
rc_get_int(struct rc_database *db, const char *key, int default_val)
{
	const char *val = rc_get_string(db, key);
	if (!val)
		return default_val;

	char *end;
	long result = strtol(val, &end, 10);
	if (end == val || *end != '\0')
		return default_val;

	return (int)result;
}

bool
rc_get_bool(struct rc_database *db, const char *key, bool default_val)
{
	const char *val = rc_get_string(db, key);
	if (!val)
		return default_val;

	if (strcasecmp(val, "true") == 0 || strcmp(val, "1") == 0 ||
	    strcasecmp(val, "yes") == 0)
		return true;
	if (strcasecmp(val, "false") == 0 || strcmp(val, "0") == 0 ||
	    strcasecmp(val, "no") == 0)
		return false;

	return default_val;
}
