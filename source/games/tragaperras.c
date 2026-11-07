#include "games/tragaperras.h"

#define C TP_CHERRY
#define L TP_LEMON
#define O TP_ORANGE
#define P TP_PLUM
#define B TP_BELL
#define R TP_BAR
#define S TP_SEVEN

const int TP_REEL[TP_REELS][TP_STRIP] = {
	{ C, L, O, P, L, B, O, L, R, P, C, O, L, S, P, O, B, L, R, O, P, L },
	{ L, O, C, P, O, L, B, P, O, R, L, O, P, C, L, S, O, P, B, L, O, R },
	{ O, P, L, C, O, B, P, L, O, R, P, L, O, S, P, L, B, O, P, L, R, O },
};

#undef C
#undef L
#undef O
#undef P
#undef B
#undef R
#undef S

const int TP_LINE[TP_LINES][TP_REELS] = {
	{ 1, 1, 1 }, /* centro */
	{ 0, 0, 0 },
	{ 2, 2, 2 },
	{ 0, 1, 2 },
	{ 2, 1, 0 },
};

int tp_line_pay(const int sym[TP_REELS])
{
	static const int THREE[TP_SYMBOLS] = { 40, 14, 14, 20, 50, 100, 300 };
	if (sym[0] == sym[1] && sym[1] == sym[2])
		return THREE[sym[0]];
	if (sym[0] == TP_CHERRY && sym[1] == TP_CHERRY)
		return 5;
	if (sym[0] == TP_CHERRY)
		return 2;
	return 0;
}

int tp_symbol_at(int r, int stop, int row)
{
	return TP_REEL[r][((stop + row - 1) % TP_STRIP + TP_STRIP) % TP_STRIP];
}

int tp_eval(const int stop[TP_REELS], int lines, int* win_mask)
{
	int total = 0, mask = 0;
	for (int l = 0; l < lines && l < TP_LINES; l++) {
		int sym[TP_REELS];
		for (int r = 0; r < TP_REELS; r++)
			sym[r] = tp_symbol_at(r, stop[r], TP_LINE[l][r]);
		int p = tp_line_pay(sym);
		if (p) {
			total += p;
			mask |= 1 << l;
		}
	}
	if (win_mask)
		*win_mask = mask;
	return total;
}

void tp_spin(Rng* rng, int stop[TP_REELS])
{
	for (int r = 0; r < TP_REELS; r++)
		stop[r] = rng_range(rng, TP_STRIP);
}

const char* tp_symbol_name(int s)
{
	static const char* const N[TP_SYMBOLS] = { "Cereza", "Limón", "Naranja", "Ciruela", "Campana", "BAR", "Siete" };
	return s >= 0 && s < TP_SYMBOLS ? N[s] : "";
}
