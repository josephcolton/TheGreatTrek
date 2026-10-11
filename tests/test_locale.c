#include "locale.h"
#include "test.h"

#include <string.h>

int main(void)
{
    char err[128] = "";
    Locale *loc = locale_load("{\"a.title\":\"Hello\",\"a.greet\":\"Goeie more\","
                              "\"a.greet@english\":\"Good morning\","
                              "\"a.greet@english.trader\":\"Morning, customer\"}",
                              err, sizeof err);
    CHECK(loc != NULL);
    CHECK(locale_count(loc) == 4);
    CHECK(strcmp(locale_get(loc, "a.title"), "Hello") == 0);
    CHECK(locale_find(loc, "missing") == NULL);
    CHECK(strcmp(locale_get(loc, "missing.key"), "missing.key") == 0);
    CHECK(strcmp(locale_get_variant(loc, "a.greet", "dutch", NULL), "Goeie more") == 0);
    CHECK(strcmp(locale_get_variant(loc, "a.greet", "english", NULL), "Good morning") == 0);
    CHECK(strcmp(locale_get_variant(loc, "a.greet", "english", "settler"), "Good morning") == 0);
    CHECK(strcmp(locale_get_variant(loc, "a.greet", "english", "trader"), "Morning, customer") == 0);
    locale_free(loc);

    CHECK(locale_load("[1,2]", err, sizeof err) == NULL);
    CHECK(locale_load("{\"x\":3}", err, sizeof err) == NULL);
    CHECK(locale_load("{\"x\":\"a\",\"x\":\"b\"}", err, sizeof err) == NULL);
    CHECK(strstr(err, "duplicate") != NULL);
    return TEST_REPORT();
}
