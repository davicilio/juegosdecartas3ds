#include "ui/widgets.h"

#include <math.h>

#include "ui/audio.h"

void button_set(Button* b, float x, float y, float w, float h, const char* label, u32 color, u32 key)
{
	b->x = x;
	b->y = y;
	b->w = w;
	b->h = h;
	b->label = label;
	b->color = color;
	b->key = key;
	b->enabled = true;
}

bool button_update(Button* b, const Input* in)
{
	if (b->pressed > 0.0f)
		b->pressed -= 1.0f / 60.0f;
	if (!b->enabled)
		return false;
	bool hit = input_touch_in(in, b->x, b->y, b->w, b->h) || (b->key && (in->down & b->key));
	if (hit) {
		b->pressed = 0.15f;
		audio_play(SFX_CONFIRM);
	}
	return hit;
}

static u32 shade(u32 color, float f)
{
	/* C2D_Color32 guarda los canales como 0xAABBGGRR */
	int r = (int)((color & 0xFF) * f);
	int g = (int)(((color >> 8) & 0xFF) * f);
	int bl = (int)(((color >> 16) & 0xFF) * f);
	r = r > 255 ? 255 : r;
	g = g > 255 ? 255 : g;
	bl = bl > 255 ? 255 : bl;
	return C2D_Color32(r, g, bl, (color >> 24) & 0xFF);
}

void button_draw(const Button* b, bool highlight)
{
	float push = b->pressed > 0.0f ? 2.0f : 0.0f;
	float y = b->y + push;
	u32 base = b->enabled ? b->color : C2D_Color32(110, 110, 110, 255);

	if (highlight && b->enabled) {
		float pulse = 0.5f + 0.5f * sinf(gfx_time() * 6.0f);
		gfx_rrect(b->x - 3, y - 3, b->w + 6, b->h + 6, 10, 0, C2D_Color32(255, 220, 90, (u8)(120 + 120 * pulse)));
	}
	if (!push)
		gfx_rrect(b->x, b->y + 3, b->w, b->h, 8, 0, C2D_Color32(0, 0, 0, 110));
	gfx_rrect(b->x, y, b->w, b->h, 8, 0, shade(base, 0.55f));
	gfx_rrect(b->x + 2, y + 2, b->w - 4, b->h - 4, 6, 0, base);
	gfx_rrect(b->x + 3, y + 3, b->w - 6, (b->h - 6) * 0.48f, 5, 0, C2D_Color32(255, 255, 255, 60));

	float scale = b->h >= 28 ? 0.6f : 0.5f;
	gfx_text(b->label, b->x + b->w / 2, y + b->h / 2 - scale * 15.0f, 0, scale,
		b->enabled ? COLOR_WHITE : C2D_Color32(200, 200, 200, 255), C2D_AlignCenter);
}

bool banner_draw(float time, float cx, float cy, float w, float h)
{
	float t = fminf(1.0f, time / 0.35f);
	float hh = h * ease_out_back(t);
	gfx_rrect(cx - w / 2, cy - hh / 2, w, hh, 10, DEPTH_BANNER, C2D_Color32(0, 0, 0, 195));
	return t >= 0.6f;
}

float approach(float value, float target, float speed, float dt)
{
	float k = 1.0f - expf(-speed * dt);
	return value + (target - value) * k;
}

float ease_out_back(float t)
{
	if (t >= 1.0f)
		return 1.0f;
	const float c1 = 1.70158f, c3 = c1 + 1.0f;
	float u = t - 1.0f;
	return 1.0f + c3 * u * u * u + c1 * u * u;
}

void vis_spawn(VisCard* v, Card c, float x, float y, float scale, bool face_up, float delay)
{
	v->card = c;
	v->active = true;
	v->x = v->tx = x;
	v->y = v->ty = y;
	v->flip = v->tflip = face_up ? 1.0f : 0.0f;
	v->scale = v->tscale = scale;
	v->angle = v->tangle = 0.0f;
	v->depth = DEPTH_CARDS;
	v->delay = delay;
	v->dim = 0.0f;
	v->sound = true;
}

void vis_target(VisCard* v, float tx, float ty, float tscale, bool face_up)
{
	v->tx = tx;
	v->ty = ty;
	v->tscale = tscale;
	v->tflip = face_up ? 1.0f : 0.0f;
}

void vis_update(VisCard* v, float dt)
{
	if (!v->active)
		return;
	if (v->delay > 0.0f) {
		v->delay -= dt;
		return;
	}
	if (v->sound) {
		v->sound = false;
		audio_play(SFX_CARD);
	}
	float old_flip = v->flip;
	v->x = approach(v->x, v->tx, 11.0f, dt);
	v->y = approach(v->y, v->ty, 11.0f, dt);
	v->scale = approach(v->scale, v->tscale, 10.0f, dt);
	v->angle = approach(v->angle, v->tangle, 10.0f, dt);
	/* El giro va a velocidad constante para que se vea la animación. */
	if (v->flip < v->tflip)
		v->flip = fminf(v->tflip, v->flip + dt * 3.5f);
	else if (v->flip > v->tflip)
		v->flip = fmaxf(v->tflip, v->flip - dt * 3.5f);
	if ((old_flip - 0.5f) * (v->flip - 0.5f) < 0.0f)
		audio_play(SFX_FLIP);
}

bool vis_moving(const VisCard* v)
{
	return v->active && (v->delay > 0.0f || fabsf(v->x - v->tx) > 1.5f || fabsf(v->y - v->ty) > 1.5f ||
		v->flip != v->tflip);
}

void vis_draw(const VisCard* v)
{
	if (!v->active || v->delay > 0.0f)
		return;
	/* Mientras vuela o gira, la carta "sale" de la pantalla en 3D. */
	float dist = fabsf(v->x - v->tx) + fabsf(v->y - v->ty);
	float lift = fminf(1.0f, dist / 60.0f);
	if (v->flip != v->tflip)
		lift = fmaxf(lift, 0.8f);
	float depth = v->depth + lift * (DEPTH_LIFTED - v->depth);
	float scale = v->scale * (1.0f + 0.12f * lift);
	gfx_card(v->card, v->x, v->y, scale, v->flip, v->angle, depth, v->dim, true);
}
