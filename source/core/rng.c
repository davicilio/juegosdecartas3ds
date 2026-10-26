#include "core/rng.h"

void rng_seed(Rng* rng, uint32_t seed)
{
	rng->state = seed ? seed : 0x9E3779B9u;
}

uint32_t rng_next(Rng* rng)
{
	uint32_t x = rng->state;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	rng->state = x;
	return x;
}

int rng_range(Rng* rng, int n)
{
	if (n <= 1)
		return 0;
	return (int)(rng_next(rng) % (uint32_t)n);
}
