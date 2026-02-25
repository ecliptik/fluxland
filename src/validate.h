/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 *
 * validate.h - Configuration validation types and declarations
 *
 * Provides offline validation of config files (init, keys, style)
 * to catch errors without starting the compositor.
 */

#ifndef WM_VALIDATE_H
#define WM_VALIDATE_H

#include <stdbool.h>

struct config_error {
	int line;
	char *file;
	char *message;
	bool fatal;
};

struct config_result {
	struct config_error *errors;
	int count;
	int capacity;
};

/* Initialize a config_result */
void config_result_init(struct config_result *result);

/* Add an error (fatal=true) */
void config_result_add_error(struct config_result *result,
	const char *file, int line, const char *fmt, ...);

/* Add a warning (fatal=false) */
void config_result_add_warning(struct config_result *result,
	const char *file, int line, const char *fmt, ...);

/* Free all allocated error messages */
void config_result_destroy(struct config_result *result);

/* Return true if any fatal errors present */
bool config_result_has_errors(const struct config_result *result);

/* Print all errors/warnings to stderr */
void config_result_print(const struct config_result *result);

/* Per-subsystem validators: validate the file at path, appending to result */
void validate_init_file(const char *path, struct config_result *result);
void validate_keys_file(const char *path, struct config_result *result);
void validate_style_file(const char *path, struct config_result *result);

/* Top-level: validate all config files in config_dir. Returns 0 if valid. */
int validate_config(const char *config_dir);

#endif /* WM_VALIDATE_H */
