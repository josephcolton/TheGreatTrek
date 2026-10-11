/* PCG32 random number generator: the XSH-RR 64/32 algorithm by Melissa O'Neill
 * (pcg-random.org). Every random draw in the game goes through a TrekRng passed in
 * explicitly, so a run can be replayed from its seed. */
#ifndef TREK_RNG_H
#define TREK_RNG_H

#include <stdbool.h>
#include <stdint.h>

typedef struct TrekRng {
    uint64_t state;
    uint64_t inc;
} TrekRng;

void     rng_seed(TrekRng *rng, uint64_t seed, uint64_t stream);
uint32_t rng_next(TrekRng *rng);
/* Uniform integer in [0, bound). Returns 0 when bound is 0. */
uint32_t rng_below(TrekRng *rng, uint32_t bound);
/* Uniform integer in [lo, hi] inclusive. */
int32_t  rng_range(TrekRng *rng, int32_t lo, int32_t hi);
/* Uniform double in [0, 1). */
double   rng_unit(TrekRng *rng);
/* True with probability p (clamped to [0, 1]). */
bool     rng_chance(TrekRng *rng, double p);
/* Index chosen with probability proportional to weights[i]; -1 if all weights are <= 0. */
int      rng_weighted(TrekRng *rng, const double *weights, int count);

#endif
