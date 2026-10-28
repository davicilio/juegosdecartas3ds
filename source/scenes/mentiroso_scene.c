/* Mentiroso: echa cartas boca abajo diciendo lo que son... o acusa al anterior. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/mentiroso.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/table.h"
#include "ui/widgets.h"

#define HAND_SC 0.62f

static Mentiroso s_game;
static Table s_table;
static bool s_started;
static bool s_sel[DECK_SIZE];
static int s_cursor;
static int s_rank_pos; /* número que vas a decir (posición 0..9) al empezar tanda */
static float s_cpu_timer, s_reveal_time, s_end_time;
static bool s_recorded;
static Button s_btn_play, s_btn_call, s_btn_menu;

static const char* plural_rank(int rank, int n)
{
	static const char* const one[13] = { "", "as", "dos", "tres", "cuatro", "cinco", "seis", "siete", "", "", "sota",
		"caballo", "rey" };
	static const char* const many[13] = { "", "ases", "doses", "treses", "cuatros", "cincos", "seises", "sietes", "",
		"", "sotas", "caballos", "reyes" };
	return n == 1 ? one[rank] : many[rank];
}

static void clear_sel(void)
{
	memset(s_sel, 0, sizeof s_sel);
}

static void new_game(void)
{
	men_new_game(&s_game);
	clear_sel();
	s_cursor = 0;
	s_cpu_timer = s_reveal_time = s_end_time = 0.0f;
	s_recorded = false;
	table_message(&s_table, "Empieza %s", s_game.current == 0 ? "tú" : table_name(&s_table, s_game.current));
}

static void men_enter(void)
{
	if (!s_started) {
		table_init(&s_table, CPU_MANOLO, CPU_CARMEN, CPU_PACO);
		men_init(&s_game, (uint32_t)time(NULL) ^ 0x11E);
		s_started = true;
		new_game();
	}
	button_set(&s_btn_menu, 258, 6, 54, 20, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
}

static void announce_play(int p, int n)
{
	char buf[48];
	snprintf(buf, sizeof buf, "%s %s", n == 1 ? "Un" : n == 2 ? "Dos" : "Tres", plural_rank(s_game.rank, n));
	if (n == 1 && (s_game.rank == RANK_SOTA))
		snprintf(buf, sizeof buf, "Una sota");
	table_bubble(&s_table, p, buf);
	table_message(&s_table, "%s: \"%s\"", p == 0 ? "Tú" : table_name(&s_table, p), buf);
	audio_play(SFX_CARD);
}

static void call(void)
{
	int accuser = s_game.current;
	men_call(&s_game);
	table_bubble(&s_table, accuser, "¡Mentiroso!");
	s_reveal_time = 0.0f;
	audio_play(SFX_FLIP);
	if (s_game.was_lie) {
		table_message(&s_table, "¡%s mentía! Se lleva el montón", s_game.accused == 0 ? "Tú" : table_name(&s_table, s_game.accused));
		table_mood(&s_table, s_game.accused, EXPR_SAD, 2.0f);
		table_mood(&s_table, accuser, EXPR_HAPPY, 2.0f);
	} else {
		table_message(&s_table, "Decía la verdad: %s se lleva el montón",
			accuser == 0 ? "te" : table_name(&s_table, accuser));
		table_mood(&s_table, accuser, EXPR_SAD, 2.0f);
		table_mood(&s_table, s_game.accused, EXPR_HAPPY, 2.0f);
	}
}

static int selected(int* idx)
{
	int n = 0;
	for (int i = 0; i < s_game.hand_count[0] && n < 4; i++)
		if (s_sel[i])
			idx[n++] = i;
	return n;
}

static SceneId men_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;
	const float y = 205, h = 28;
	if (s_game.phase == MEN_PHASE_PLAYING) {
		bool mine = s_game.current == 0;
		button_set(&s_btn_play, 12, y, 144, h, GLYPH_A " Echar", C2D_Color32(40, 140, 60, 255), KEY_A);
		button_set(&s_btn_call, 164, y, 144, h, GLYPH_X " ¡Mentiroso!", C2D_Color32(190, 60, 50, 255), KEY_X);
		s_btn_play.enabled = mine;
		s_btn_call.enabled = mine && men_can_call(&s_game);
		if (mine) {
			int n = s_game.hand_count[0];
			if (n > 0) {
				if (in->repeat & KEY_LEFT)
					s_cursor = (s_cursor + n - 1) % n;
				if (in->repeat & KEY_RIGHT)
					s_cursor = (s_cursor + 1) % n;
				if (s_cursor >= n)
					s_cursor = n - 1;
			}
			if (s_game.rank == 0) {
				if (in->down & KEY_L)
					s_rank_pos = (s_rank_pos + 9) % 10;
				if (in->down & KEY_R)
					s_rank_pos = (s_rank_pos + 1) % 10;
			}
			int t = hand_touched(in, n, HAND_SC);
			int idx[4];
			if (t >= 0 || (in->down & KEY_B)) {
				int k = t >= 0 ? t : s_cursor;
				s_cursor = k;
				if (!s_sel[k] && selected(idx) >= MEN_MAX_PLAY) {
					audio_play(SFX_ERROR);
					table_message(&s_table, "Como mucho tres cartas");
				} else {
					s_sel[k] = !s_sel[k];
					audio_play(SFX_SELECT);
				}
			} else if (button_update(&s_btn_call, in)) {
				call();
			} else if (button_update(&s_btn_play, in)) {
				int count = selected(idx);
				if (count == 0) {
					/* Con A y nada elegido, se elige la carta señalada */
					s_sel[s_cursor] = true;
					count = selected(idx);
				}
				int rank = rank_from_pos(s_rank_pos);
				if (men_play(&s_game, idx, count, rank)) {
					if (s_game.phase == MEN_PHASE_PLAYING)
						announce_play(0, count);
					clear_sel();
				}
			}
		} else {
			s_cpu_timer += dt;
			if (s_cpu_timer > 1.0f) {
				s_cpu_timer = 0.0f;
				int idx[MEN_MAX_PLAY], n, rank;
				int p = s_game.current;
				if (men_ai_turn(&s_game, idx, &n, &rank))
					call();
				else if (men_play(&s_game, idx, n, rank) && s_game.phase == MEN_PHASE_PLAYING)
					announce_play(p, n);
			}
		}
	} else if (s_game.phase == MEN_PHASE_REVEAL) {
		s_btn_play.enabled = s_btn_call.enabled = false;
		s_reveal_time += dt;
		if (s_reveal_time > 2.2f || ((in->down & KEY_A) && s_reveal_time > 0.5f)) {
			men_continue(&s_game);
			clear_sel();
			if (s_game.phase == MEN_PHASE_PLAYING)
				table_message(&s_table, "Nueva tanda: empieza %s",
					s_game.current == 0 ? "tú (L/R para elegir qué dices)" : table_name(&s_table, s_game.current));
		}
	} else {
		button_set(&s_btn_play, 12, y, 144, h, GLYPH_A " Otra partida", C2D_Color32(40, 140, 60, 255), KEY_A);
		button_set(&s_btn_call, 164, y, 144, h, GLYPH_B " Menú", C2D_Color32(190, 60, 50, 255), KEY_B);
		s_btn_play.enabled = s_btn_call.enabled = s_end_time > 0.6f;
		if (!s_recorded) {
			s_recorded = true;
			profile_record_game(GAME_MENTIROSO, s_game.winner == 0);
			audio_play(s_game.winner == 0 ? SFX_WIN : SFX_LOSE);
			for (int p = 0; p < MEN_PLAYERS; p++)
				table_mood(&s_table, p, p == s_game.winner ? EXPR_HAPPY : EXPR_SAD, 5.0f);
		}
		s_end_time += dt;
		if (button_update(&s_btn_play, in))
			new_game();
		else if (button_update(&s_btn_call, in))
			return SCENE_MENU;
	}
	table_update(&s_table, dt);
	return SCENE_MENTIROSO;
}

static void men_draw_top(void)
{
	gfx_background(BG_TABLE_TOP);
	for (int p = 1; p < MEN_PLAYERS; p++) {
		char info[16];
		snprintf(info, sizeof info, "%d cartas", s_game.hand_count[p]);
		table_draw_seat(&s_table, p, s_game.hand_count[p], s_game.current == p && s_game.phase == MEN_PHASE_PLAYING,
			info);
	}
	/* Montón */
	int layers = (s_game.pile_count + 2) / 3;
	for (int i = 0; i < layers && i < 12; i++)
		gfx_card_back(200 + (i % 3 - 1) * 4.0f, 118 - i * 1.2f, 0.6f, 0.6f, (i % 4 - 1.5f) * 0.15f,
			DEPTH_CARDS + i * 0.03f, NULL);
	if (s_game.phase == MEN_PHASE_REVEAL) {
		/* Se levantan las cartas acusadas */
		for (int i = 0; i < s_game.last_n; i++)
			gfx_card(s_game.last[i], 200 + (i - (s_game.last_n - 1) / 2.0f) * 44.0f, 118, 0.7f, 1.0f, 0.0f,
				DEPTH_LIFTED, s_game.last[i].rank != s_game.rank ? 0.0f : 0.0f, true);
		gfx_text(s_game.was_lie ? "¡Mentira!" : "Verdad", 200, 162, DEPTH_BANNER, 0.6f,
			s_game.was_lie ? COLOR_RED : COLOR_GOLD, C2D_AlignCenter);
	} else if (s_game.rank) {
		gfx_textf(200, 154, DEPTH_UI, 0.5f, COLOR_GOLD, C2D_AlignCenter, "Se dice: %s",
			plural_rank(s_game.rank, 2));
		gfx_textf(200, 170, DEPTH_UI, 0.42f, COLOR_CREAM, C2D_AlignCenter, "%d en el montón", s_game.pile_count);
	}
	char me[24];
	snprintf(me, sizeof me, "Tus cartas: %d", s_game.hand_count[0]);
	table_draw_me(&s_table, me);
	table_draw_bubbles(&s_table);

	if (s_game.phase == MEN_PHASE_GAME_OVER && banner_draw(s_end_time, 200, 112, 300, 64)) {
		if (s_game.winner == 0)
			gfx_text("¡Te has quedado sin cartas!", 200, 96, DEPTH_BANNER, 0.62f, COLOR_GOLD, C2D_AlignCenter);
		else
			gfx_textf(200, 96, DEPTH_BANNER, 0.62f, COLOR_RED, C2D_AlignCenter, "Gana %s",
				table_name(&s_table, s_game.winner));
	}
}

static void men_draw_bottom(void)
{
	gfx_background(BG_TABLE_BOTTOM);
	table_draw_message(&s_table);
	button_draw(&s_btn_menu, false);
	bool mine = s_game.current == 0 && s_game.phase == MEN_PHASE_PLAYING;
	if (mine) {
		if (s_game.rank == 0)
			gfx_textf(BOTTOM_W / 2, 30, 0, 0.5f, COLOR_GOLD, C2D_AlignCenter, GLYPH_L " Vas a decir: %s " GLYPH_R,
				plural_rank(rank_from_pos(s_rank_pos), 2));
		else
			gfx_textf(BOTTOM_W / 2, 30, 0, 0.5f, COLOR_GOLD, C2D_AlignCenter, "Di que son %s (o acusa)",
				plural_rank(s_game.rank, 2));
	}
	hand_draw(s_game.hands[0], s_game.hand_count[0], HAND_SC, s_sel, mine ? s_cursor : -1, NULL);
	gfx_text("Toca (o " GLYPH_B ") para elegir hasta tres cartas", BOTTOM_W / 2, 172, 0, 0.42f,
		C2D_Color32(200, 230, 200, 255), C2D_AlignCenter);
	button_draw(&s_btn_play, s_btn_play.enabled);
	button_draw(&s_btn_call, s_btn_call.enabled);
}

const Scene scene_mentiroso = { men_enter, men_update, men_draw_top, men_draw_bottom };
