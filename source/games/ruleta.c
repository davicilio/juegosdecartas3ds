#include "games/ruleta.h"

const int RL_WHEEL[RL_NUMBERS] = { 0, 32, 15, 19, 4, 21, 2, 25, 17, 34, 6, 27, 13, 36, 11, 30, 8, 23, 10,
	5, 24, 16, 33, 1, 20, 14, 31, 9, 22, 18, 29, 7, 28, 12, 35, 3, 26 };

bool rl_is_red(int n)
{
	static const unsigned long long RED = (1ull << 1) | (1ull << 3) | (1ull << 5) | (1ull << 7) | (1ull << 9) |
		(1ull << 12) | (1ull << 14) | (1ull << 16) | (1ull << 18) | (1ull << 19) | (1ull << 21) | (1ull << 23) |
		(1ull << 25) | (1ull << 27) | (1ull << 30) | (1ull << 32) | (1ull << 34) | (1ull << 36);
	return n > 0 && n < RL_NUMBERS && ((RED >> n) & 1);
}

bool rl_wins(int spot, int n)
{
	if (spot < RL_NUMBERS)
		return spot == n;
	if (n == 0)
		return false;
	if (spot < RL_COLUMN0)
		return (n - 1) / 12 == spot - RL_DOZEN0;
	if (spot < RL_LOW)
		return (n - 1) % 3 == spot - RL_COLUMN0;
	switch (spot) {
	case RL_LOW:
		return n <= 18;
	case RL_HIGH:
		return n >= 19;
	case RL_EVEN:
		return n % 2 == 0;
	case RL_ODD:
		return n % 2 == 1;
	case RL_RED:
		return rl_is_red(n);
	case RL_BLACK:
		return !rl_is_red(n);
	}
	return false;
}

int rl_pays(int spot)
{
	return spot < RL_NUMBERS ? 35 : spot < RL_LOW ? 2 : 1;
}

int rl_payout(const int bets[RL_SPOTS], int n)
{
	int total = 0;
	for (int s = 0; s < RL_SPOTS; s++)
		if (bets[s] && rl_wins(s, n))
			total += bets[s] * (rl_pays(s) + 1);
	return total;
}

int rl_spin(Rng* rng)
{
	return rng_range(rng, RL_NUMBERS);
}
