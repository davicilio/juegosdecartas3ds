#include "ui/flyers.h"

#define FLYERS_MAX 64

static VisCard s_cards[FLYERS_MAX];
static FlyScreen s_screen[FLYERS_MAX];

void flyers_clear(void)
{
	for (int i = 0; i < FLYERS_MAX; i++)
		s_cards[i].active = false;
}

void flyers_add(const VisCard* v, float tx, float ty, float delay, bool face_up, FlyScreen screen)
{
	for (int i = 0; i < FLYERS_MAX; i++) {
		if (s_cards[i].active)
			continue;
		s_cards[i] = *v;
		s_cards[i].active = true;
		s_cards[i].delay = delay;
		s_cards[i].dim = 0.0f;
		vis_target(&s_cards[i], tx, ty, v->tscale, face_up);
		s_screen[i] = screen;
		return;
	}
}

void flyers_update(float dt)
{
	for (int i = 0; i < FLYERS_MAX; i++) {
		vis_update(&s_cards[i], dt);
		if (s_cards[i].active && !vis_moving(&s_cards[i]))
			s_cards[i].active = false;
	}
}

void flyers_draw(FlyScreen screen)
{
	for (int i = 0; i < FLYERS_MAX; i++)
		if (s_screen[i] == screen)
			vis_draw(&s_cards[i]);
}

bool flyers_busy(void)
{
	for (int i = 0; i < FLYERS_MAX; i++)
		if (s_cards[i].active)
			return true;
	return false;
}
