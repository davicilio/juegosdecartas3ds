/* La mona: roba a ciegas una carta al siguiente y empareja; pierde quien se
 * queda con la mona. */
#include <math.h>
#include <stdio.h>
#include <time.h>

#include "games/mona.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/table.h"
#include "ui/widgets.h"

#define PICK_Y     78.0f
#define PICK_SCALE 0.62f
#define MINE_Y     168.0f
#define MINE_SCALE 0.5f

static Mona s_game;
static Table s_table;
static bool s_started;
static int s_cursor;
static float s_cpu_timer, s_end_time;
static bool s_recorded;
static Button s_btn_take, s_btn_again, s_btn_menu;

static float pick_x(int i, int n)
{
	float step = n > 1 ? fminf(44.0f, 260.0f / (n - 1)) : 0.0f;
	return BOTTOM_W / 2.0f + (i - (n - 1) / 2.0f) * step;
}

static float mine_x(int i, int n)
{
	float step = n > 1 ? fminf(36.0f, 270.0f / (n - 1)) : 0.0f;
	return BOTTOM_W / 2.0f + (i - (n - 1) / 2.0f) * step;
}

static void new_game(void)
{
	mona_new_game(&s_game);
	s_cursor = 0;
	s_cpu_timer = s_end_time = 0.0f;
	s_recorded = false;
	table_message(&s_table, "Habéis echado las parejas. Empieza %s",
		s_game.current == 0 ? "tú" : table_name(&s_table, s_game.current));
}

static void mona_enter(void)
{
	if (!s_started) {
		table_init(&s_table, CPU_PEPE, CPU_LOLA, CPU_CARMEN);
		mona_init(&s_game, (uint32_t)time(NULL) ^ 0x30A);
		s_started = true;
		new_game();
	}
	button_set(&s_btn_menu, 258, 6, 54, 20, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
	button_set(&s_btn_take, 12, 205, 144, 28, GLYPH_A " Robar esta", C2D_Color32(40, 140, 60, 255), KEY_A);
	button_set(&s_btn_again, 164, 205, 144, 28, GLYPH_Y " Nueva partida", C2D_Color32(60, 90, 160, 255), KEY_Y);
}

static void take(int index)
{
	int p = s_game.current, v = mona_victim(&s_game);
	if (!mona_take(&s_game, index))
		return;
	audio_play(SFX_CARD);
	char name[32];
	card_describe_article(s_game.last_card, name, sizeof name);
	if (p == 0)
		table_message(&s_table, s_game.last_paired ? "Robas %s: ¡pareja!" : "Robas %s", name);
	else if (v == 0)
		table_message(&s_table, "%s te roba %s%s", table_name(&s_table, p), name, s_game.last_paired ? " y hace pareja" : "");
	else
		table_message(&s_table, "%s roba a %s%s", table_name(&s_table, p), table_name(&s_table, v),
			s_game.last_paired ? " y hace pareja" : "");
	if (s_game.last_paired) {
		table_bubble(&s_table, p, "¡Pareja!");
		table_mood(&s_table, p, EXPR_HAPPY, 1.2f);
	}
	if (s_game.last_card.rank == RANK_SOTA && !s_game.last_paired && p == 0)
		table_mood(&s_table, 0, EXPR_SURPRISED, 1.5f);
	s_cursor = 0;
}

static SceneId mona_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;
	if (button_update(&s_btn_again, in)) {
		if (!s_recorded && !mona_over(&s_game))
			profile_record_game(GAME_MONA, false);
		new_game();
		return SCENE_MONA;
	}
	if (!mona_over(&s_game)) {
		int v = mona_victim(&s_game);
		bool mine = s_game.current == 0;
		s_btn_take.enabled = mine;
		if (mine && v >= 0) {
			int n = s_game.hand_count[v];
			if (in->repeat & KEY_LEFT)
				s_cursor = (s_cursor + n - 1) % n;
			if (in->repeat & KEY_RIGHT)
				s_cursor = (s_cursor + 1) % n;
			if (s_cursor >= n)
				s_cursor = n - 1;
			int t = -1;
			if (in->touch_down)
				for (int i = n - 1; i >= 0 && t < 0; i--)
					if (fabsf(in->tx - pick_x(i, n)) <= CARD_W * PICK_SCALE / 2 &&
						fabsf(in->ty - PICK_Y) <= CARD_H * PICK_SCALE / 2)
						t = i;
			if (t >= 0)
				take(t);
			else if (button_update(&s_btn_take, in))
				take(s_cursor);
		} else if (!mine) {
			s_cpu_timer += dt;
			if (s_cpu_timer > 0.9f) {
				s_cpu_timer = 0.0f;
				take(mona_ai_pick(&s_game));
			}
		}
	} else {
		if (!s_recorded) {
			s_recorded = true;
			bool lost = s_game.loser == 0;
			profile_record_game(GAME_MONA, !lost);
			audio_play(lost ? SFX_LOSE : SFX_WIN);
			for (int p = 0; p < MONA_PLAYERS; p++)
				table_mood(&s_table, p, p == s_game.loser ? EXPR_SAD : EXPR_HAPPY, 5.0f);
		}
		s_end_time += dt;
		if (in->down & KEY_A)
			new_game();
	}
	table_update(&s_table, dt);
	return SCENE_MONA;
}

static void mona_draw_top(void)
{
	gfx_background(BG_TABLE_TOP);
	for (int p = 1; p < MONA_PLAYERS; p++) {
		char info[24];
		if (s_game.hand_count[p] == 0)
			snprintf(info, sizeof info, "¡Librado!");
		else
			snprintf(info, sizeof info, "%d cartas", s_game.hand_count[p]);
		table_draw_seat(&s_table, p, s_game.hand_count[p], s_game.current == p && !mona_over(&s_game), info);
	}
	/* Parejas en el centro */
	int pairs = 0;
	for (int p = 0; p < MONA_PLAYERS; p++)
		pairs += s_game.pairs[p];
	for (int i = 0; i < (pairs + 2) / 3 && i < 8; i++)
		gfx_card_back(200 + (i % 4) * 3.0f, 120 - i * 1.5f, 0.6f, 0.6f, (i % 3 - 1) * 0.2f, DEPTH_CARDS + i * 0.05f,
			NULL);
	gfx_textf(200, 154, DEPTH_UI, 0.45f, COLOR_CREAM, C2D_AlignCenter, "%d parejas en la mesa", pairs);
	char me[32];
	snprintf(me, sizeof me, "Tus cartas: %d", s_game.hand_count[0]);
	table_draw_me(&s_table, me);
	table_draw_bubbles(&s_table);

	if (mona_over(&s_game) && banner_draw(s_end_time, 200, 112, 320, 80)) {
		if (s_game.loser == 0)
			gfx_text("¡Te has quedado la mona!", 200, 86, DEPTH_BANNER, 0.65f, COLOR_RED, C2D_AlignCenter);
		else
			gfx_textf(200, 86, DEPTH_BANNER, 0.6f, COLOR_GOLD, C2D_AlignCenter, "%s se queda la mona",
				table_name(&s_table, s_game.loser));
		gfx_text(GLYPH_A " Otra partida", 200, 116, DEPTH_BANNER, 0.5f, COLOR_CREAM, C2D_AlignCenter);
	}
}

static void mona_draw_bottom(void)
{
	gfx_background(BG_TABLE_BOTTOM);
	table_draw_message(&s_table);
	button_draw(&s_btn_menu, false);

	int v = mona_victim(&s_game);
	if (!mona_over(&s_game) && s_game.current == 0 && v >= 0) {
		gfx_textf(BOTTOM_W / 2, 30, 0, 0.5f, COLOR_GOLD, C2D_AlignCenter, "Roba una carta a %s",
			table_name(&s_table, v));
		int n = s_game.hand_count[v];
		float pulse = 0.75f + 0.25f * sinf(gfx_time() * 6.0f);
		for (int i = 0; i < n; i++) {
			float x = pick_x(i, n), y = PICK_Y - (i == s_cursor ? 8.0f : 0.0f);
			if (i == s_cursor) {
				C2D_ImageTint tint;
				C2D_AlphaImageTint(&tint, pulse);
				gfx_sprite_ex(SPR_CARD_GLOW, x, y, PICK_SCALE, PICK_SCALE, 0.0f, 0, &tint);
			}
			gfx_card_back(x, y, PICK_SCALE, PICK_SCALE, 0.0f, 0, NULL);
		}
	}
	/* Tu mano, boca arriba */
	int n = s_game.hand_count[0];
	for (int i = 0; i < n; i++)
		gfx_card(s_game.hands[0][i], mine_x(i, n), MINE_Y, MINE_SCALE, 1.0f, 0.0f, 0, 0.0f, i == 0);
	if (n == 0 && !mona_over(&s_game))
		gfx_text("¡Te has librado!", BOTTOM_W / 2, MINE_Y - 8, 0, 0.6f, COLOR_GOLD, C2D_AlignCenter);

	if (!mona_over(&s_game) && s_game.current == 0)
		button_draw(&s_btn_take, true);
	button_draw(&s_btn_again, false);
}

/* Salir a medias desde la pausa: la próxima vez se empieza de cero */
static void scene_leave(void)
{
	s_started = false;
}

const Scene scene_mona = { mona_enter, mona_update, mona_draw_top, mona_draw_bottom, scene_leave };
