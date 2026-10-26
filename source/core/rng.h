#ifndef CORE_RNG_H
#define CORE_RNG_H

#include <stdint.h>

/* Generador pseudoaleatorio xorshift32: pequeño, rápido y determinista,
 * así las partidas son reproducibles en los tests. */
typedef struct {
	uint32_t state;
} Rng;

void rng_seed(Rng* rng, uint32_t seed);
uint32_t rng_next(Rng* rng);
/* Entero uniforme en [0, n). */
int rng_range(Rng* rng, int n);

#endif
