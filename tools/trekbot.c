/* trekbot: plays many headless runs to check balance. Usage: trekbot [runs] [seed]
 * Milestone 1 stub: exercises the RNG so the tool builds and runs. */
#include "rng.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    int runs = argc > 1 ? atoi(argv[1]) : 10;
    uint64_t seed = argc > 2 ? strtoull(argv[2], NULL, 10) : 1;
    for (int i = 0; i < runs; i++) {
        TrekRng rng;
        rng_seed(&rng, seed + (uint64_t)i, 1);
        printf("run %d: seed %llu, first draw %u\n", i, (unsigned long long)(seed + (uint64_t)i),
               (unsigned)rng_next(&rng));
    }
    return 0;
}
