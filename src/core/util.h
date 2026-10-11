/* Small helpers shared by the core: file reading and string copies. Plain C, no SDL. */
#ifndef TREK_UTIL_H
#define TREK_UTIL_H

#include <stddef.h>

/* Reads a whole file into a NUL-terminated buffer the caller frees. NULL on failure. */
char *util_read_file(const char *path, size_t *out_len);
/* Writes len bytes to path; returns 0 on success. */
int   util_write_file(const char *path, const char *data, size_t len);
/* malloc'd copy of s (NULL stays NULL). */
char *util_strdup(const char *s);
/* Joins dir and name with a '/' if needed; caller frees. */
char *util_path_join(const char *dir, const char *name);

#endif
