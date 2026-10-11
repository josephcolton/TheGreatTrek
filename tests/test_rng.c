#include "rng.h"
#include "test.h"

static void test_reproducible(void)
{
    TrekRng a, b;
    rng_seed(&a, 42, 54);
    rng_seed(&b, 42, 54);
    for (int i = 0; i < 1000; i++) {
        CHECK(rng_next(&a) == rng_next(&b));
    }
}

static void test_known_values(void)
{
    /* Reference output of pcg32 with seed 42, stream 54 (pcg32-demo). */
    TrekRng r;
    rng_seed(&r, 42, 54);
    CHECK(rng_next(&r) == 0xa15c02b7u);
    CHECK(rng_next(&r) == 0x7b47f409u);
    CHECK(rng_next(&r) == 0xba1d3330u);
}

static void test_ranges(void)
{
    TrekRng r;
    rng_seed(&r, 7, 1);
    for (int i = 0; i < 10000; i++) {
        uint32_t v = rng_below(&r, 6);
        CHECK(v < 6);
        int32_t x = rng_range(&r, -3, 3);
        CHECK(x >= -3 && x <= 3);
        double u = rng_unit(&r);
        CHECK(u >= 0.0 && u < 1.0);
    }
    CHECK(rng_below(&r, 0) == 0);
    CHECK(rng_range(&r, 5, 5) == 5);
    CHECK(!rng_chance(&r, 0.0));
    CHECK(rng_chance(&r, 1.0));
}

static void test_weighted(void)
{
    TrekRng r;
    rng_seed(&r, 99, 3);
    double w[3] = {1.0, 0.0, 3.0};
    int counts[3] = {0, 0, 0};
    for (int i = 0; i < 40000; i++) {
        int k = rng_weighted(&r, w, 3);
        CHECK(k >= 0 && k < 3);
        counts[k]++;
    }
    CHECK(counts[1] == 0);
    /* Expect about 10000 and 30000. */
    CHECK(counts[0] > 9000 && counts[0] < 11000);
    CHECK(counts[2] > 29000 && counts[2] < 31000);
    double none[2] = {0.0, -1.0};
    CHECK(rng_weighted(&r, none, 2) == -1);
}

int main(void)
{
    test_reproducible();
    test_known_values();
    test_ranges();
    test_weighted();
    return TEST_REPORT();
}
