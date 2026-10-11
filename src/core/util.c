#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *util_read_file(const char *path, size_t *out_len)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    long size = ftell(f);
    if (size < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }
    char *buf = malloc((size_t)size + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    size_t got = fread(buf, 1, (size_t)size, f);
    fclose(f);
    if (got != (size_t)size) {
        free(buf);
        return NULL;
    }
    buf[size] = '\0';
    if (out_len) {
        *out_len = (size_t)size;
    }
    return buf;
}

int util_write_file(const char *path, const char *data, size_t len)
{
    FILE *f = fopen(path, "wb");
    if (!f) {
        return -1;
    }
    size_t put = fwrite(data, 1, len, f);
    int ok = (put == len) && (fflush(f) == 0);
    if (fclose(f) != 0) {
        ok = 0;
    }
    return ok ? 0 : -1;
}

char *util_strdup(const char *s)
{
    if (!s) {
        return NULL;
    }
    size_t n = strlen(s) + 1;
    char *copy = malloc(n);
    if (copy) {
        memcpy(copy, s, n);
    }
    return copy;
}

char *util_path_join(const char *dir, const char *name)
{
    size_t dl = strlen(dir), nl = strlen(name);
    int need_sep = dl > 0 && dir[dl - 1] != '/' && dir[dl - 1] != '\\';
    char *out = malloc(dl + (size_t)need_sep + nl + 1);
    if (!out) {
        return NULL;
    }
    memcpy(out, dir, dl);
    if (need_sep) {
        out[dl] = '/';
    }
    memcpy(out + dl + (size_t)need_sep, name, nl + 1);
    return out;
}
