#include "games/rueda.h"

/* 24 unos, 15 doses, 7 cincos, 4 dieces, 2 veintes y 2 comodines */
const int RD_WHEEL[RD_SLOTS] = {
	RD_JOKER, RD_2, RD_1, RD_5, RD_1, RD_2, RD_10, RD_1, RD_2, RD_1, RD_1, RD_5, RD_2, RD_20, RD_1, RD_2, RD_1,
	RD_1, RD_5, RD_2, RD_10, RD_1, RD_1, RD_2, RD_1, RD_1, RD_5, RD_STAR, RD_2, RD_1, RD_2, RD_1, RD_1, RD_10,
	RD_5, RD_2, RD_1, RD_2, RD_1, RD_1, RD_20, RD_2, RD_5, RD_1, RD_2, RD_1, RD_10, RD_1, RD_2, RD_5, RD_1, RD_2,
	RD_1, RD_1,
};

int rd_pays(int symbol)
{
	static const int P[RD_SYMBOLS] = { 1, 2, 5, 10, 20, 40, 40 };
	return symbol >= 0 && symbol < RD_SYMBOLS ? P[symbol] : 0;
}

int rd_spin(Rng* rng)
{
	return rng_range(rng, RD_SLOTS);
}

int rd_payout(const int bets[RD_SYMBOLS], int slot)
{
	int s = RD_WHEEL[slot];
	return bets[s] * (rd_pays(s) + 1);
}

const char* rd_label(int symbol)
{
	static const char* const L[RD_SYMBOLS] = { "1", "2", "5", "10", "20", "Comodín", "Estrella" };
	return symbol >= 0 && symbol < RD_SYMBOLS ? L[symbol] : "";
}
