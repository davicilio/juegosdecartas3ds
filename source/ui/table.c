#include "ui/table.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "ui/gfx.h"
#include "ui/widgets.h"

static const float SEAT_X[TABLE_PLAYERS] = { 200, 44, 200, 356 };
static const float SEAT_Y[TABLE_PLAYERS] = { 290, 78, 30, 78 };

void table_init(Table* t, CpuId left, CpuId top, CpuId right)
{
	memset(t, 0, sizeof *t);
	t->cpu[0] = CPU_COUNT;
	t->cpu[1] = left;
	t->cpu[2] = top;
	t->cpu[3] = right;
}

const char* table_name(const Table* t, int p)
{
	return p == 0 ? profile_get()->avatar.name : cpu_name(t->cpu[p]);
}

const Avatar* table_avatar(const Table* t, int p)
{
	return p == 0 ? &profile_get()->avatar : cpu_avatar(t->cpu[p]);
}

void table_message(Table* t, const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(t->message, sizeof t->message, fmt, ap);
	va_end(ap);
	t->message_flash = 1.0f;
}

void table_bubble(Table* t, int p, const char* text)
{
	snprintf(t->bubble[p], sizeof t->bubble[p], "%s", text);
	t->bubble_time[p] = 1.8f;
}

void table_mood(Table* t, int p, Expression e, float seconds)
{
	mood_set(&t->mood[p], e, seconds);
}

void table_update(Table* t, float dt)
{
	for (int p = 0; p < TABLE_PLAYERS; p++) {
		mood_update(&t->mood[p], dt);
		if (t->bubble_time[p] > 0.0f)
			t->bubble_time[p] -= dt;
	}
	if (t->message_flash > 0.0f)
		t->message_flash -= dt * 2.0f;
}

float table_seat_x(int p)
{
	return SEAT_X[p % TABLE_PLAYERS];
}

float table_seat_y(int p)
{
	return SEAT_Y[p % TABLE_PLAYERS];
}

void table_draw_seat(const Table* t, int p, int cards, bool turn, const char* info)
{
	float x = SEAT_X[p], y = SEAT_Y[p];
	int shown = cards > 10 ? 10 : cards;
	for (int i = 0; i < shown; i++) {
		float off = i - (shown - 1) / 2.0f;
		if (p == 2)
			gfx_card_back(x + 50 + off * 9.0f, y, 0.4f, 0.4f, off * 0.05f, DEPTH_CARDS + i * 0.02f, NULL);
		else
			gfx_card_back(x + off * 3.0f, y + 48 + off * 3.0f, 0.4f, 0.4f, (p == 1 ? 1.0f : -1.0f) * (0.3f + off * 0.04f),
				DEPTH_CARDS + i * 0.02f, NULL);
	}
	float ax = p == 2 ? x - 40 : x;
	mii_draw(table_avatar(t, p), ax, y, 17, DEPTH_UI + 0.5f, mood_expr(&t->mood[p]), false);
	gfx_panel(ax - 38, y + 20, 76, info ? 32 : 18, DEPTH_UI, C2D_Color32(10, 50, 25, 220),
		turn ? COLOR_GOLD : C2D_Color32(190, 60, 50, 255));
	gfx_text(table_name(t, p), ax, y + 22, DEPTH_UI, 0.42f, COLOR_CREAM, C2D_AlignCenter);
	if (info)
		gfx_text(info, ax, y + 36, DEPTH_UI, 0.4f, COLOR_GOLD, C2D_AlignCenter);
}

void table_draw_me(const Table* t, const char* info)
{
	mii_draw(table_avatar(t, 0), 30, 212, 15, DEPTH_UI + 0.5f, mood_expr(&t->mood[0]), false);
	if (info)
		gfx_text(info, 52, 204, DEPTH_UI, 0.45f, COLOR_CREAM, C2D_AlignLeft);
}

void table_draw_bubbles(const Table* t)
{
	for (int p = 0; p < TABLE_PLAYERS; p++) {
		if (t->bubble_time[p] <= 0.0f)
			continue;
		float bx = p == 0 ? 120 : p == 2 ? 290 : SEAT_X[p] + (p == 1 ? 44 : -44);
		float by = p == 0 ? 196 : SEAT_Y[p] - 4;
		float pop = ease_out_back(fminf(1.0f, (1.8f - t->bubble_time[p]) / 0.2f));
		float w = gfx_text_width(t->bubble[p], 0.5f) + 16;
		gfx_rrect(bx - w / 2 * pop, by - 11 * pop, w * pop, 22 * pop, 8, DEPTH_BANNER, C2D_Color32(255, 250, 235, 240));
		if (pop > 0.8f)
			gfx_text(t->bubble[p], bx, by - 8, DEPTH_BANNER, 0.5f, C2D_Color32(120, 20, 20, 255), C2D_AlignCenter);
	}
}

void table_draw_message(const Table* t)
{
	gfx_text(t->message, 10, 10, 0, 0.45f, t->message_flash > 0.0f ? COLOR_GOLD : COLOR_CREAM, C2D_AlignLeft);
}

float hand_x(int i, int n, float scale)
{
	float w = CARD_W * scale;
	float step = n > 1 ? fminf(w + 4.0f, (304.0f - w) / (n - 1)) : 0.0f;
	return BOTTOM_W / 2.0f + (i - (n - 1) / 2.0f) * step;
}

int hand_touched(const Input* in, int n, float scale)
{
	if (!in->touch_down)
		return -1;
	float w = CARD_W * scale / 2.0f, h = CARD_H * scale / 2.0f;
	for (int i = n - 1; i >= 0; i--) {
		float x = hand_x(i, n, scale);
		if (in->tx >= x - w && in->tx <= x + w && in->ty >= HAND_Y - h - 14 && in->ty <= HAND_Y + h)
			return i;
	}
	return -1;
}

void hand_draw(const Card* cards, int n, float scale, const bool* sel, int cursor, const bool* playable)
{
	float pulse = 0.75f + 0.25f * sinf(gfx_time() * 6.0f);
	for (int i = 0; i < n; i++) {
		float x = hand_x(i, n, scale);
		float y = HAND_Y - ((sel && sel[i]) ? 14.0f : 0.0f);
		if (i == cursor) {
			C2D_ImageTint tint;
			C2D_AlphaImageTint(&tint, pulse);
			gfx_sprite_ex(SPR_CARD_GLOW, x, y, scale, scale, 0.0f, 0, &tint);
		}
		bool dim = playable && !playable[i];
		gfx_card(cards[i], x, y, scale, 1.0f, 0.0f, 0, dim ? 0.45f : 0.0f, i == 0);
	}
}
