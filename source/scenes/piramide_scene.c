/* Solitario en pirámide: la pirámide en la pantalla táctil y el mazo arriba. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/piramide.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/flyers.h"
#include "ui/gfx.h"
#include "ui/mii.h"
#include "ui/widgets.h"

#define PYR_SCALE 0.6f
#define PYR_DX    42.0f
#define PYR_DY    23.0f
#define PYR_TOP   38.0f
#define STOCK_X   70.0f
#define STOCK_Y   120.0f
#define WASTE_X   200.0f
#define WASTE_Y   120.0f
#define WASTE_SCALE 1.15f

static Piramide s_game;
static bool s_started;
static int s_cursor;   /* carta de la pirámide señalada con la cruceta */
static int s_selected; /* primera carta de una pareja (-1 ninguna) */
static VisCard s_drawn; /* la última carta sacada, volando hacia los descartes */
static Mood s_mood;
static float s_end_time;
static bool s_recorded;
static char s_message[96];
static float s_message_flash;
static Button s_btn_draw, s_btn_menu, s_btn_again;

static void set_message(const char* msg)
{
	snprintf(s_message, sizeof s_message, "%s", msg);
	s_message_flash = 1.0f;
}

static void slot_pos(int index, float* x, float* y)
{
	int row = pyr_row(index);
	int pos = index - pyr_index(row, 0);
	*x = BOTTOM_W / 2.0f + (pos - row / 2.0f) * PYR_DX;
	*y = PYR_TOP + row * PYR_DY;
}

/* Cartas descubiertas, de abajo arriba y de izquierda a derecha (para la cruceta) */
static int exposed_list(int* out)
{
	int n = 0;
	for (int row = PYR_ROWS - 1; row >= 0; row--)
		for (int pos = 0; pos <= row; pos++)
			if (pyr_exposed(&s_game, pyr_index(row, pos)))
				out[n++] = pyr_index(row, pos);
	return n;
}

static void fix_cursor(void)
{
	int list[PYR_CARDS];
	int n = exposed_list(list);
	if (n == 0) {
		s_cursor = -1;
		return;
	}
	for (int i = 0; i < n; i++)
		if (list[i] == s_cursor)
			return;
	s_cursor = list[0];
}

static void new_game(void)
{
	pyr_new_game(&s_game);
	memset(&s_drawn, 0, sizeof s_drawn);
	flyers_clear();
	s_selected = -1;
	s_cursor = -1;
	s_end_time = 0.0f;
	s_recorded = false;
	fix_cursor();
	set_message("Saca una carta del mazo con " GLYPH_X);
}

static void pyr_scene_enter(void)
{
	if (!s_started) {
		pyr_init(&s_game, (uint32_t)time(NULL) ^ 0x9124AD);
		s_started = true;
		new_game();
	}
	button_set(&s_btn_menu, 258, 4, 54, 20, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
	button_set(&s_btn_draw, 6, 210, 132, 26, GLYPH_X " Sacar carta", C2D_Color32(40, 140, 60, 255), KEY_X);
	button_set(&s_btn_again, 182, 210, 132, 26, GLYPH_Y " Nueva partida", C2D_Color32(60, 90, 160, 255), KEY_Y);
}

static void remove_visual(int index, float delay)
{
	float x, y;
	slot_pos(index, &x, &y);
	VisCard v;
	vis_spawn(&v, s_game.card[index], x, y, PYR_SCALE, true, 0.0f);
	flyers_add(&v, BOTTOM_W / 2.0f, -70.0f, delay, true, FLY_BOTTOM);
}

static void draw_card(void)
{
	bool new_pass = s_game.stock_count == 0;
	if (!pyr_draw(&s_game)) {
		audio_play(SFX_ERROR);
		return;
	}
	Card c;
	pyr_current(&s_game, &c);
	vis_spawn(&s_drawn, c, STOCK_X, STOCK_Y, 1.0f, false, 0.0f);
	vis_target(&s_drawn, WASTE_X, WASTE_Y, WASTE_SCALE, true);
	s_selected = -1;
	char buf[64];
	if (new_pass)
		snprintf(buf, sizeof buf, "Pasada %d de %d del mazo", s_game.pass, PYR_PASSES);
	else
		snprintf(buf, sizeof buf, "Busca en la base: %s", rank_name(c.rank));
	set_message(buf);
}

/* Elegir una carta de la pirámide: quitarla con la del mazo, o hacer pareja */
static void choose(int index)
{
	if (!pyr_exposed(&s_game, index)) {
		audio_play(SFX_ERROR);
		set_message("Esa carta está tapada");
		return;
	}
	if (s_selected >= 0 && s_selected != index) {
		int first = s_selected;
		if (pyr_pair(&s_game, first, index)) {
			remove_visual(first, 0.0f);
			remove_visual(index, 0.08f);
			audio_play(SFX_CONFIRM);
			set_message("¡Pareja!");
			s_selected = -1;
			return;
		}
	}
	if (pyr_can_take(&s_game, index)) {
		pyr_take(&s_game, index);
		remove_visual(index, 0.0f);
		audio_play(SFX_CONFIRM);
		s_selected = -1;
		set_message("¡Fuera!");
		return;
	}
	if (s_selected == index) {
		s_selected = -1;
		return;
	}
	s_selected = index;
	audio_play(SFX_SELECT);
	char buf[64], name[32];
	card_describe_article(s_game.card[index], name, sizeof name);
	snprintf(buf, sizeof buf, "Elige otra carta igual que %s", name);
	set_message(buf);
}

static int touched(const Input* in)
{
	if (!in->touch_down)
		return -1;
	/* De abajo arriba: las cartas de la base tapan a las de encima */
	for (int i = PYR_CARDS - 1; i >= 0; i--) {
		if (s_game.removed[i])
			continue;
		float x, y;
		slot_pos(i, &x, &y);
		float w = CARD_W * PYR_SCALE / 2.0f, h = CARD_H * PYR_SCALE / 2.0f;
		if (in->tx >= x - w && in->tx <= x + w && in->ty >= y - h && in->ty <= y + h)
			return i;
	}
	return -1;
}

static void move_cursor(int dir)
{
	int list[PYR_CARDS];
	int n = exposed_list(list);
	if (n == 0)
		return;
	int k = 0;
	for (int i = 0; i < n; i++)
		if (list[i] == s_cursor)
			k = i;
	s_cursor = list[(k + dir + n) % n];
	audio_play(SFX_SELECT);
}

static SceneId pyr_scene_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;
	bool over = pyr_won(&s_game) || pyr_stuck(&s_game);
	s_btn_draw.enabled = !over && pyr_can_draw(&s_game);
	s_btn_again.enabled = true;

	if (button_update(&s_btn_again, in)) {
		if (!over && !s_recorded && s_game.moves > 0)
			profile_record_game(GAME_PIRAMIDE, false); /* abandonada: cuenta como perdida */
		new_game();
		return SCENE_PIRAMIDE;
	}
	if (!over) {
		if (button_update(&s_btn_draw, in)) {
			draw_card();
		} else {
			if (in->repeat & (KEY_RIGHT | KEY_UP))
				move_cursor(1);
			if (in->repeat & (KEY_LEFT | KEY_DOWN))
				move_cursor(-1);
			if (in->down & KEY_B)
				s_selected = -1;
			int t = touched(in);
			if (t >= 0) {
				s_cursor = pyr_exposed(&s_game, t) ? t : s_cursor;
				choose(t);
			} else if ((in->down & KEY_A) && s_cursor >= 0) {
				choose(s_cursor);
			}
		}
		fix_cursor();
		if (s_selected >= 0 && !pyr_exposed(&s_game, s_selected))
			s_selected = -1;
	}

	over = pyr_won(&s_game) || pyr_stuck(&s_game);
	if (over && !flyers_busy()) {
		if (!s_recorded) {
			s_recorded = true;
			bool won = pyr_won(&s_game);
			profile_record_game(GAME_PIRAMIDE, won);
			audio_play(won ? SFX_WIN : SFX_LOSE);
			mood_set(&s_mood, won ? EXPR_HAPPY : EXPR_SAD, 5.0f);
			set_message(won ? "¡Has vaciado la pirámide!" : "No quedan jugadas");
		}
		s_end_time += dt;
		if (in->down & KEY_A)
			new_game();
	}

	vis_update(&s_drawn, dt);
	flyers_update(dt);
	mood_update(&s_mood, dt);
	if (s_message_flash > 0.0f)
		s_message_flash -= dt * 2.0f;
	return SCENE_PIRAMIDE;
}

static void pyr_scene_draw_top(void)
{
	gfx_background(BG_TABLE_TOP);

	/* Mazo */
	int layers = (s_game.stock_count + 2) / 3;
	for (int i = 0; i < layers; i++)
		gfx_card_back(STOCK_X - i * 0.5f, STOCK_Y - i * 0.8f, 1.0f, 1.0f, 0.0f, DEPTH_CARDS + i * 0.1f, NULL);
	if (s_game.stock_count == 0) {
		gfx_rrect(STOCK_X - 32, STOCK_Y - 48, 64, 96, 6, DEPTH_TABLE, C2D_Color32(0, 0, 0, 60));
		if (pyr_can_draw(&s_game))
			gfx_text("Dar la vuelta", STOCK_X, STOCK_Y - 8, DEPTH_UI, 0.45f, COLOR_CREAM, C2D_AlignCenter);
	}
	gfx_textf(STOCK_X, STOCK_Y + 54, DEPTH_UI, 0.45f, COLOR_CREAM, C2D_AlignCenter, "Mazo: %d", s_game.stock_count);
	gfx_textf(STOCK_X, STOCK_Y + 70, DEPTH_UI, 0.42f, COLOR_GOLD, C2D_AlignCenter, "Pasada %d de %d", s_game.pass,
		PYR_PASSES);

	/* Descartes: la carta actual, grande */
	if (s_game.waste_count > 1) {
		Card under = s_game.waste[s_game.waste_count - 2];
		gfx_card(under, WASTE_X, WASTE_Y, WASTE_SCALE, 1.0f, -0.05f, DEPTH_CARDS, 0.3f, true);
	}
	if (s_drawn.active && s_game.waste_count > 0 && card_equal(s_drawn.card, s_game.waste[s_game.waste_count - 1]))
		vis_draw(&s_drawn);
	else if (s_game.waste_count > 0)
		gfx_card(s_game.waste[s_game.waste_count - 1], WASTE_X, WASTE_Y, WASTE_SCALE, 1.0f, 0.0f, DEPTH_CARDS + 0.5f,
			0.0f, true);
	else
		gfx_rrect(WASTE_X - 37, WASTE_Y - 55, 74, 110, 6, DEPTH_TABLE, C2D_Color32(0, 0, 0, 60));

	/* Marcador */
	gfx_panel(286, 40, 104, 120, DEPTH_UI, C2D_Color32(10, 50, 25, 210), C2D_Color32(20, 60, 30, 255));
	mii_draw(&profile_get()->avatar, 338, 72, 20, DEPTH_UI + 0.4f, mood_expr(&s_mood), false);
	gfx_text("Quedan", 338, 100, DEPTH_UI, 0.45f, COLOR_CREAM, C2D_AlignCenter);
	gfx_textf(338, 116, DEPTH_UI, 0.8f, COLOR_GOLD, C2D_AlignCenter, "%d", s_game.left);
	gfx_text("Solitario en pirámide", 200, 8, DEPTH_UI, 0.55f, COLOR_GOLD, C2D_AlignCenter);
	gfx_panel(20, 208, 360, 24, DEPTH_UI, C2D_Color32(10, 50, 25, 200), C2D_Color32(20, 60, 30, 255));
	gfx_text(s_message, 200, 212, DEPTH_UI, 0.45f, s_message_flash > 0.0f ? COLOR_GOLD : COLOR_CREAM, C2D_AlignCenter);

	if (s_recorded) {
		if (banner_draw(s_end_time, 200, 120, 300, 64)) {
			bool won = pyr_won(&s_game);
			gfx_text(won ? "¡Pirámide vaciada!" : "Sin jugadas", 200, 92, DEPTH_BANNER, 0.8f,
				won ? COLOR_GOLD : COLOR_RED, C2D_AlignCenter);
			gfx_textf(200, 122, DEPTH_BANNER, 0.5f, COLOR_CREAM, C2D_AlignCenter,
				won ? "En %d jugadas.  " GLYPH_A " Otra partida" : "Quedan %d cartas.  " GLYPH_A " Otra partida",
				won ? s_game.moves : s_game.left);
		}
	}
}

static void pyr_scene_draw_bottom(void)
{
	gfx_background(BG_TABLE_BOTTOM);
	button_draw(&s_btn_menu, false);

	Card cur;
	bool has_cur = pyr_current(&s_game, &cur);
	float pulse = 0.6f + 0.4f * sinf(gfx_time() * 6.0f);
	for (int i = 0; i < PYR_CARDS; i++) {
		if (s_game.removed[i])
			continue;
		float x, y;
		slot_pos(i, &x, &y);
		bool up = pyr_exposed(&s_game, i);
		if (up && (i == s_cursor || i == s_selected)) {
			C2D_ImageTint tint;
			C2D_AlphaImageTint(&tint, i == s_selected ? 1.0f : pulse);
			gfx_sprite_ex(SPR_CARD_GLOW, x, y, PYR_SCALE, PYR_SCALE, 0.0f, 0, &tint);
		}
		gfx_card(s_game.card[i], x, y, PYR_SCALE, up ? 1.0f : 0.0f, 0.0f, 0, 0.0f, true);
		if (up && has_cur && cur.rank == s_game.card[i].rank)
			gfx_rrect(x - 8, y + 18, 16, 6, 3, 0, C2D_Color32(255, 215, 90, (u8)(255 * pulse)));
	}
	flyers_draw(FLY_BOTTOM);

	button_draw(&s_btn_draw, s_btn_draw.enabled);
	button_draw(&s_btn_again, false);
}

/* Salir a medias desde la pausa: la próxima vez se empieza de cero */
static void scene_leave(void)
{
	s_started = false;
}

const Scene scene_piramide = { pyr_scene_enter, pyr_scene_update, pyr_scene_draw_top, pyr_scene_draw_bottom, scene_leave };
