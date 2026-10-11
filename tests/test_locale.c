#include "format.h"
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

    /* pronouns follow the person: {who.his}, {who.He}; a plain name falls back to they */
    loc = locale_load("{\"t\":\"{who} lost {who.his} hat. {who.He} found it; {helper} helped {who.him}.\","
                      "\"pronoun.he.he\":\"he\",\"pronoun.he.him\":\"him\",\"pronoun.he.his\":\"his\","
                      "\"pronoun.she.he\":\"she\",\"pronoun.she.him\":\"her\",\"pronoun.she.his\":\"her\","
                      "\"pronoun.they.he\":\"they\",\"pronoun.they.him\":\"them\",\"pronoun.they.his\":\"their\"}",
                      err, sizeof err);
    CHECK(loc != NULL);
    Member james = {.name = "James", .pronoun = PRONOUN_HE}, sarah = {.name = "Sarah", .pronoun = PRONOUN_SHE};
    LocText t;
    char out[200];
    loctext_set(&t, "t");
    loctext_arg_person(&t, "who", &james);
    loctext_arg_person(&t, "helper", &sarah);
    fmt_text(loc, &t, NULL, NULL, out, sizeof out);
    CHECK(strcmp(out, "James lost his hat. He found it; Sarah helped him.") == 0);
    loctext_set(&t, "t");
    loctext_arg_person(&t, "who", &sarah);
    loctext_arg_person(&t, "helper", &james);
    fmt_text(loc, &t, NULL, NULL, out, sizeof out);
    CHECK(strcmp(out, "Sarah lost her hat. She found it; James helped her.") == 0);
    loctext_set(&t, "t");
    loctext_arg_str(&t, "who", "Klaas"); /* a save from before pronouns */
    loctext_arg_str(&t, "helper", "Dina");
    fmt_text(loc, &t, NULL, NULL, out, sizeof out);
    CHECK(strcmp(out, "Klaas lost their hat. They found it; Dina helped them.") == 0);
    locale_free(loc);

    CHECK(locale_load("[1,2]", err, sizeof err) == NULL);
    CHECK(locale_load("{\"x\":3}", err, sizeof err) == NULL);
    CHECK(locale_load("{\"x\":\"a\",\"x\":\"b\"}", err, sizeof err) == NULL);
    CHECK(strstr(err, "duplicate") != NULL);
    return TEST_REPORT();
}
