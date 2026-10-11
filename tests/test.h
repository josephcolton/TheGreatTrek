/* A very small test harness: CHECK records failures, TEST_MAIN reports them. */
#ifndef TREK_TEST_H
#define TREK_TEST_H

#include <stdio.h>

static int test_failures = 0;
static int test_checks = 0;

#define CHECK(cond)                                                              \
    do {                                                                         \
        test_checks++;                                                           \
        if (!(cond)) {                                                           \
            test_failures++;                                                     \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
        }                                                                        \
    } while (0)

#define TEST_REPORT()                                                            \
    (printf("%d checks, %d failures\n", test_checks, test_failures),             \
     test_failures == 0 ? 0 : 1)

#endif
