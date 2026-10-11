#include "rng.h"

#define PCG_MULT 6364136223846793005ULL

void rng_seed(TrekRng *rng, uint64_t seed, uint64_t stream)
{
    rng->state = 0;
    rng->inc = (stream << 1u) | 1u;
    rng_next(rng);
    rng->state += seed;
    rng_next(rng);
}

uint32_t rng_next(TrekRng *rng)
{
    uint64_t old = rng->state;
    rng->state = old * PCG_MULT + rng->inc;
    uint32_t xorshifted = (uint32_t)(((old >> 18u) ^ old) >> 27u);
    uint32_t rot = (uint32_t)(old >> 59u);
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31u));
}

uint32_t rng_below(TrekRng *rng, uint32_t bound)
{
    if (bound == 0) {
        return 0;
    }
    /* Reject the low values that would bias the modulo. */
    uint32_t threshold = (uint32_t)(-bound) % bound;
    for (;;) {
        uint32_t r = rng_next(rng);
        if (r >= threshold) {
            return r % bound;
        }
    }
}

int32_t rng_range(TrekRng *rng, int32_t lo, int32_t hi)
{
    if (hi <= lo) {
        return lo;
    }
    uint32_t span = (uint32_t)((int64_t)hi - (int64_t)lo + 1);
    return (int32_t)((int64_t)lo + rng_below(rng, span));
}

double rng_unit(TrekRng *rng)
{
    /* 53 random bits from two draws. */
    uint64_t hi = rng_next(rng) >> 5;
    uint64_t lo = rng_next(rng) >> 6;
    return (double)((hi << 26) | lo) * (1.0 / 9007199254740992.0);
}

bool rng_chance(TrekRng *rng, double p)
{
    if (p <= 0.0) {
        return false;
    }
    if (p >= 1.0) {
        return true;
    }
    return rng_unit(rng) < p;
}

int rng_weighted(TrekRng *rng, const double *weights, int count)
{
    double total = 0.0;
    for (int i = 0; i < count; i++) {
        if (weights[i] > 0.0) {
            total += weights[i];
        }
    }
    if (total <= 0.0) {
        return -1;
    }
    double pick = rng_unit(rng) * total;
    int last = -1;
    for (int i = 0; i < count; i++) {
        if (weights[i] <= 0.0) {
            continue;
        }
        last = i;
        if (pick < weights[i]) {
            return i;
        }
        pick -= weights[i];
    }
    return last; /* floating-point rounding: fall back to the last valid entry */
}
