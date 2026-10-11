/* datacheck: validates the files in data/. Usage: datacheck [data_dir]
 * Milestone 1: checks that the locale file parses. Later: events, items, route, sources. */
#include "locale.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    const char *data_dir = argc > 1 ? argv[1] : "data";
    int errors = 0;

    char *path = util_path_join(data_dir, "locale/en.json");
    char *text = util_read_file(path, NULL);
    if (!text) {
        fprintf(stderr, "error: cannot read %s\n", path);
        errors++;
    } else {
        char err[256] = "";
        Locale *loc = locale_load(text, err, sizeof err);
        if (!loc) {
            fprintf(stderr, "error: %s: %s\n", path, err);
            errors++;
        } else {
            printf("%s: %lu keys\n", path, (unsigned long)locale_count(loc));
            locale_free(loc);
        }
        free(text);
    }
    free(path);

    printf(errors ? "datacheck: %d error(s)\n" : "datacheck: ok\n", errors);
    return errors ? 1 : 0;
}
