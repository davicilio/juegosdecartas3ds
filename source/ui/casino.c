#include "ui/casino.h"

#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/wallet.h"

static const int VALUES[CASINO_CHIP_KINDS] = { 1, 5, 25, 100, 500 };

#define PICK_X0   20.0f
#define PICK_STEP 30.0f

int casino_chip_value(int idx)
{
	return VALUES[idx < 0 ? 0 : idx >= CASINO_CHIP_KINDS ? CASINO_CHIP_KINDS - 1 : idx];
}

void casino_pick_update(int* idx, const Input* in, float y)
{
	int old = *idx;
	if (in->down & KEY_L)
		*idx = (*idx + CASINO_CHIP_KINDS - 1) % CASINO_CHIP_KINDS;
	if (in->down & KEY_R)
		*idx = (*idx + 1) % CASINO_CHIP_KINDS;
	if (in->touch_down)
		for (int i = 0; i < CASINO_CHIP_KINDS; i++)
			if (input_touch_in(in, PICK_X0 + i * PICK_STEP - 13, y - 13, 26, 26))
				*idx = i;
	if (*idx != old)
		audio_play(SFX_CHIP);
}

static int sprite_for(int amount)
{
	return amount >= 100 ? SPR_CHIP_BLACK : amount >= 25 ? SPR_CHIP_GREEN : amount >= 5 ? SPR_CHIP_BLUE : SPR_CHIP_RED;
}

void casino_chip(int amount, float cx, float cy, float scale, float depth)
{
	if (amount <= 0)
		return;
	gfx_ellipse(cx + 1.5f * scale, cy + 2 * scale, 12 * scale, 12 * scale, depth, C2D_Color32(0, 0, 0, 90));
	gfx_sprite_ex(sprite_for(amount), cx, cy, scale, scale, 0.0f, depth, NULL);
	if (amount >= 500)
		gfx_ellipse(cx, cy, 7.5f * scale, 7.5f * scale, depth, C2D_Color32(120, 40, 150, 255));
	float ts = amount >= 1000 ? 0.32f : amount >= 100 ? 0.36f : 0.42f;
	gfx_textf_plain(cx, cy - 15.0f * ts * scale * 1.2f, depth, ts * scale * 1.2f, COLOR_WHITE, C2D_AlignCenter, "%d", amount);
}

void casino_pick_draw(int idx, float y, int staked)
{
	gfx_rrect(4, y - 16, BOTTOM_W - 8, 32, 8, 0, C2D_Color32(0, 0, 0, 90));
	for (int i = 0; i < CASINO_CHIP_KINDS; i++) {
		float x = PICK_X0 + i * PICK_STEP;
		bool on = i == idx;
		if (on)
			gfx_ellipse(x, y, 15, 15, 0, COLOR_GOLD);
		casino_chip(VALUES[i], x, y - (on ? 2 : 0), on ? 1.05f : 0.9f, 0);
	}
	gfx_text(GLYPH_L GLYPH_R, PICK_X0 + CASINO_CHIP_KINDS * PICK_STEP - 6, y - 7, 0, 0.42f, COLOR_CREAM, C2D_AlignLeft);
	gfx_textf(BOTTOM_W - 10, y - 14, 0, 0.45f, COLOR_GOLD, C2D_AlignRight, "Fichas: %d", wallet_chips());
	gfx_textf(BOTTOM_W - 10, y + 1, 0, 0.42f, COLOR_CREAM, C2D_AlignRight, "En la mesa: %d", staked);
}

bool casino_place(int* bet, int value)
{
	int got = wallet_take(value);
	if (got <= 0) {
		audio_play(SFX_ERROR);
		return false;
	}
	*bet += got;
	audio_play(SFX_CHIP);
	return true;
}

void casino_refund(int* bets, int n)
{
	int total = 0;
	for (int i = 0; i < n; i++) {
		total += bets[i];
		bets[i] = 0;
	}
	if (total)
		wallet_give(total);
}

int casino_sum(const int* bets, int n)
{
	int t = 0;
	for (int i = 0; i < n; i++)
		t += bets[i];
	return t;
}

bool casino_repeat(int* bets, const int* last, int n)
{
	int need = casino_sum(last, n);
	if (need <= 0 || wallet_chips() < need) {
		audio_play(SFX_ERROR);
		return false;
	}
	casino_refund(bets, n);
	wallet_take(need);
	for (int i = 0; i < n; i++)
		bets[i] = last[i];
	audio_play(SFX_CHIP);
	return true;
}
