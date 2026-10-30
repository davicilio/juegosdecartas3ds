/* Tresillo (3 jugadores) y cuatrillo (4). */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/tresillo.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/table.h"
#include "ui/widgets.h"

static const char* const BID_NAME[4] = { "Paso", "Entrada", "Vuelta", "Solo" };
static const char* const SUIT_CAP[NUM_SUITS] = { "Oros", "Copas", "Espadas", "Bastos" };
static const char* const RESULT[4] = { "¡Sacada!", "Puesta", "¡Codillo!", "Nadie juega" };
/* Asiento de cada jugador: con tres, el 1 a la izquierda y el 2 a la derecha */
static const int SEAT[2][TS_MAX_PLAYERS] = { { 0, 1, 3, 3 }, { 0, 1, 2, 3 } };
static const float TRICK_X[TABLE_PLAYERS] = { 200, 154, 200, 246 };
static const float TRICK_Y[TABLE_PLAYERS] = { 150, 114, 80, 114 };

static Tresillo s_games[2];
static Table s_tables[2];
static bool s_started[2];
static int s_v; /* 0 tresillo, 1 cuatrillo */
#define G       (&s_games[s_v])
#define s_table s_tables[s_v]
static bool s_sel[TS_MAX_HAND];
static int s_cursor, s_pick_trump = -1;
static float s_cpu_timer, s_trick_timer, s_end_time;
static TsPhase s_prev_phase;
static bool s_king_seen;
static Button s_btn_bid[4], s_btn_suit[NUM_SUITS + 1], s_btn_main, s_btn_alt, s_btn_menu;

#define HAND_SC (s_v ? 0.55f : 0.6f)

static int seat(int p)
{
	return SEAT[s_v][p];
}

static const char* who(int p)
{
	return p == 0 ? "Tú" : table_name(&s_table, seat(p));
}

static SceneId scene_id(void)
{
	return s_v ? SCENE_CUATRILLO : SCENE_TRESILLO;
}

static void start_hand(void)
{
	memset(s_sel, 0, sizeof s_sel);
	s_cursor = 0;
	s_pick_trump = -1;
	s_king_seen = false;
	s_cpu_timer = s_trick_timer = s_end_time = 0.0f;
	s_prev_phase = G->phase;
	table_message(&s_table, "Mano %d de %d. Puja primero %s", G->hands_played + 1, TS_HANDS,
		G->mano == 0 ? "tú" : who(G->mano));
}

static void enter_variant(int v)
{
	s_v = v;
	if (!s_started[v]) {
		if (v)
			table_init(&s_table, CPU_RAMON, CPU_LUCIA, CPU_PACO);
		else
			table_init(&s_table, CPU_MANOLO, CPU_COUNT, CPU_CARMEN);
		ts_init(G, v ? 4 : 3, (uint32_t)time(NULL) ^ (0x7E5 + v * 31));
		s_started[v] = true;
		start_hand();
	}
	button_set(&s_btn_menu, 258, 6, 54, 20, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
	static const u32 keys[4] = { KEY_B, KEY_A, KEY_X, KEY_Y };
	static const char* const labels[4] = { GLYPH_B " Paso", GLYPH_A " Entrada", GLYPH_X " Vuelta", GLYPH_Y " Solo" };
	for (int i = 0; i < 4; i++)
		button_set(&s_btn_bid[i], 12 + (i % 2) * 152, 170 + (i / 2) * 35, 144, 28, labels[i],
			i == 0 ? C2D_Color32(110, 110, 110, 255) : C2D_Color32(40, 140, 60, 255), keys[i]);
	for (int i = 0; i < NUM_SUITS; i++)
		button_set(&s_btn_suit[i], 12 + (i % 2) * 152, 120 + (i / 2) * 34, 144, 28, SUIT_CAP[i],
			C2D_Color32(60, 90, 160, 255), 0);
	button_set(&s_btn_suit[NUM_SUITS], 88, 190, 144, 28, "Sin compañero", C2D_Color32(110, 110, 110, 255), 0);
}

static void tresillo_enter(void)
{
	enter_variant(0);
}

static void cuatrillo_enter(void)
{
	enter_variant(1);
}

static void after_bid(void)
{
	int p = G->last_bidder;
	table_bubble(&s_table, seat(p), BID_NAME[G->last_bid]);
	audio_play(SFX_SELECT);
	if (G->phase == TS_HAND_OVER || G->phase == TS_GAME_OVER)
		table_message(&s_table, "Pasan todos: se reparte de nuevo");
	else if (G->phase != TS_BIDDING) {
		table_message(&s_table, "%s %s con %s", G->hombre == 0 ? "Eres" : who(G->hombre), G->hombre == 0 ? "el hombre" : "es el hombre",
			BID_NAME[G->bid]);
		if (G->bid == TS_VUELTA)
			table_message(&s_table, "%s juega de vuelta: pinta %s", who(G->hombre), suit_name((Suit)G->trump));
	}
}

static void after_choose(void)
{
	table_bubble(&s_table, seat(G->hombre), SUIT_CAP[G->trump]);
	if (G->called_king >= 0)
		table_message(&s_table, "Triunfo: %s. Llama el rey de %s", suit_name((Suit)G->trump), suit_name((Suit)G->called_king));
	else
		table_message(&s_table, "Triunfo: %s", suit_name((Suit)G->trump));
	audio_play(SFX_CHIP);
}

static void after_discard(int p)
{
	char buf[24];
	snprintf(buf, sizeof buf, G->changed[p] ? "Cambio %d" : "Servido", G->changed[p]);
	table_bubble(&s_table, seat(p), buf);
	audio_play(SFX_CARD);
	if (G->phase == TS_PLAYING)
		table_message(&s_table, "Sale %s", G->current == 0 ? "tú" : who(G->current));
}

static void play(int p, int i)
{
	Card c = G->hands[p][i];
	if (!ts_play(G, i))
		return;
	audio_play(SFX_CARD);
	if (G->called_king >= 0 && c.suit == G->called_king && c.rank == RANK_REY && !s_king_seen) {
		s_king_seen = true;
		table_bubble(&s_table, seat(p), "¡Soy yo!");
		table_message(&s_table, "%s es el compañero del hombre", who(p));
	}
	if (G->phase == TS_TRICK_DONE)
		s_trick_timer = 0.0f;
}

static void cpu_turn(int p)
{
	switch (G->phase) {
	case TS_BIDDING:
		ts_bid(G, ts_ai_bid(G, p));
		after_bid();
		break;
	case TS_TRUMP: {
		int t, k;
		ts_ai_choose(G, p, &t, &k);
		ts_choose(G, t, k);
		after_choose();
		break;
	}
	case TS_DISCARD: {
		bool sel[TS_MAX_HAND];
		ts_ai_discard(G, p, sel);
		ts_discard(G, sel);
		after_discard(p);
		break;
	}
	case TS_PLAYING:
		play(p, ts_ai_play(G, p));
		break;
	default:
		break;
	}
}

static SceneId ts_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;
	int n = G->hand_n[0];
	bool mine = G->current == 0;
	const float y = 205, h = 28;
	switch (G->phase) {
	case TS_BIDDING:
		if (mine)
			for (int i = 0; i < 4; i++) {
				s_btn_bid[i].enabled = ts_can_bid(G, i);
				if (s_btn_bid[i].enabled && button_update(&s_btn_bid[i], in)) {
					ts_bid(G, i);
					after_bid();
					break;
				}
			}
		break;
	case TS_TRUMP:
		if (mine) {
			if (s_pick_trump < 0) {
				for (int i = 0; i < NUM_SUITS; i++)
					if (button_update(&s_btn_suit[i], in)) {
						s_pick_trump = i;
						if (!(G->players == 4 && G->bid == TS_ENTRADA) && ts_choose(G, i, -1))
							after_choose();
					}
			} else {
				for (int i = 0; i <= NUM_SUITS; i++) {
					int king = i == NUM_SUITS ? -1 : i;
					s_btn_suit[i].enabled = ts_can_call(G, king);
					if (s_btn_suit[i].enabled && button_update(&s_btn_suit[i], in) && ts_choose(G, s_pick_trump, king)) {
						after_choose();
						break;
					}
				}
			}
		}
		break;
	case TS_DISCARD: {
		int count = 0;
		for (int i = 0; i < n; i++)
			count += s_sel[i];
		static char label[32];
		snprintf(label, sizeof label, count ? GLYPH_A " Cambiar %d" : GLYPH_A " Servido", count);
		button_set(&s_btn_main, 12, y, 296, h, label, C2D_Color32(40, 140, 60, 255), KEY_A);
		s_btn_main.enabled = mine;
		if (mine) {
			if (in->repeat & KEY_LEFT)
				s_cursor = (s_cursor + n - 1) % n;
			if (in->repeat & KEY_RIGHT)
				s_cursor = (s_cursor + 1) % n;
			int t = hand_touched(in, n, HAND_SC);
			if (t >= 0 || (in->down & KEY_B)) {
				int k = t >= 0 ? t : s_cursor;
				s_cursor = k;
				if (s_sel[k] || count < G->talon_n)
					s_sel[k] = !s_sel[k];
				audio_play(SFX_SELECT);
			} else if (button_update(&s_btn_main, in)) {
				ts_discard(G, s_sel);
				memset(s_sel, 0, sizeof s_sel);
				after_discard(0);
			}
		}
		break;
	}
	case TS_PLAYING:
		button_set(&s_btn_main, 12, y, 296, h, GLYPH_A " Echar", C2D_Color32(40, 140, 60, 255), KEY_A);
		s_btn_main.enabled = mine;
		if (mine) {
			if (in->repeat & KEY_LEFT)
				s_cursor = (s_cursor + n - 1) % n;
			if (in->repeat & KEY_RIGHT)
				s_cursor = (s_cursor + 1) % n;
			if (s_cursor >= n)
				s_cursor = n - 1;
			if (!ts_can_play(G, 0, s_cursor))
				for (int i = 0; i < n; i++)
					if (ts_can_play(G, 0, i)) {
						s_cursor = i;
						break;
					}
			int t = hand_touched(in, n, HAND_SC);
			int pick = t >= 0 ? t : button_update(&s_btn_main, in) ? s_cursor : -1;
			if (pick >= 0) {
				if (ts_can_play(G, 0, pick))
					play(0, pick);
				else {
					audio_play(SFX_ERROR);
					table_message(&s_table, "Hay que asistir al palo que ha salido");
				}
			}
		}
		break;
	case TS_TRICK_DONE:
		s_trick_timer += dt;
		if (s_trick_timer > 1.0f) {
			int w = G->trick_winner;
			ts_collect(G);
			if (w == 0)
				table_message(&s_table, "Te llevas la baza");
			else
				table_message(&s_table, "%s se lleva la baza", who(w));
		}
		break;
	case TS_HAND_OVER:
	case TS_GAME_OVER: {
		bool over = G->phase == TS_GAME_OVER;
		button_set(&s_btn_main, 12, y, 144, h, over ? GLYPH_A " Otra partida" : GLYPH_A " Siguiente mano",
			C2D_Color32(40, 140, 60, 255), KEY_A);
		button_set(&s_btn_alt, 164, y, 144, h, GLYPH_B " Menú", C2D_Color32(190, 60, 50, 255), KEY_B);
		s_end_time += dt;
		s_btn_main.enabled = s_btn_alt.enabled = s_end_time > 0.6f;
		if (button_update(&s_btn_main, in)) {
			if (over)
				ts_new_game(G);
			else
				ts_new_hand(G);
			start_hand();
		} else if (button_update(&s_btn_alt, in)) {
			return SCENE_MENU;
		}
		break;
	}
	}
	if (!mine && G->phase <= TS_PLAYING) {
		s_cpu_timer += dt;
		if (s_cpu_timer > 0.85f) {
			s_cpu_timer = 0.0f;
			cpu_turn(G->current);
		}
	}
	if (G->phase != s_prev_phase) {
		if (G->phase >= TS_HAND_OVER) {
			s_end_time = 0.0f;
			for (int p = 0; p < G->players; p++)
				if (G->gain[p])
					table_mood(&s_table, seat(p), G->gain[p] > 0 ? EXPR_HAPPY : EXPR_SAD, 3.0f);
			if (G->phase == TS_GAME_OVER) {
				bool won = ts_winner(G) == 0;
				profile_record_game(s_v ? GAME_CUATRILLO : GAME_TRESILLO, won);
				audio_play(won ? SFX_WIN : SFX_LOSE);
			}
		}
		s_prev_phase = G->phase;
	}
	table_update(&s_table, dt);
	return scene_id();
}

static void ts_draw_top(void)
{
	gfx_background(BG_TABLE_TOP);
	for (int p = 1; p < G->players; p++) {
		char info[32];
		if (G->phase >= TS_PLAYING)
			snprintf(info, sizeof info, "%s%d bazas", p == G->hombre ? "Hombre · " : "", G->tricks[p]);
		else
			snprintf(info, sizeof info, "%s%d puntos", p == G->hombre ? "Hombre · " : "", G->score[p]);
		bool turn = G->current == p && G->phase <= TS_PLAYING;
		table_draw_seat(&s_table, seat(p), G->hand_n[p], turn, info);
	}
	if (G->phase == TS_PLAYING || G->phase == TS_TRICK_DONE)
		for (int k = 0; k < G->played_in_trick; k++) {
			int p = (G->leader + k) % G->players;
			gfx_card(G->trick[p], TRICK_X[seat(p)], TRICK_Y[seat(p)], 0.55f, 1.0f, (seat(p) - 1.5f) * 0.06f,
				DEPTH_CARDS + k * 0.1f, 0.0f, true);
		}
	/* Panel: triunfo, puja y monte */
	gfx_panel(306, 150, 88, 82, DEPTH_UI, C2D_Color32(10, 50, 25, 210), C2D_Color32(20, 60, 30, 255));
	if (G->trump >= 0) {
		gfx_sprite(SPR_SUIT_FIRST + G->trump, 326, 162, 0.5f, DEPTH_UI);
		gfx_text(SUIT_CAP[G->trump], 362, 154, DEPTH_UI, 0.42f, COLOR_GOLD, C2D_AlignCenter);
	}
	if (G->hombre >= 0)
		gfx_textf(350, 174, DEPTH_UI, 0.42f, C2D_Color32(200, 230, 200, 255), C2D_AlignCenter, "%s", BID_NAME[G->bid]);
	if (G->called_king >= 0)
		gfx_textf(350, 192, DEPTH_UI, 0.4f, COLOR_CREAM, C2D_AlignCenter, "Rey de %s", suit_name((Suit)G->called_king));
	else if (G->players == 3)
		gfx_textf(350, 192, DEPTH_UI, 0.4f, COLOR_CREAM, C2D_AlignCenter, "Monte: %d", G->talon_n);
	gfx_textf(350, 210, DEPTH_UI, 0.4f, C2D_Color32(200, 230, 200, 255), C2D_AlignCenter, "Mano %d/%d",
		G->hands_played + (G->phase < TS_HAND_OVER), TS_HANDS);
	char me[40];
	if (G->phase >= TS_PLAYING && G->phase <= TS_TRICK_DONE)
		snprintf(me, sizeof me, "%sBazas: %d", G->hombre == 0 ? "Hombre · " : "", G->tricks[0]);
	else
		snprintf(me, sizeof me, "Tus puntos: %d", G->score[0]);
	table_draw_me(&s_table, me);
	table_draw_bubbles(&s_table);

	if ((G->phase == TS_HAND_OVER || G->phase == TS_GAME_OVER) && banner_draw(s_end_time, 200, 112, 340, 104)) {
		if (G->phase == TS_GAME_OVER) {
			int w = ts_winner(G);
			gfx_text(w == 0 ? "¡Ganas la partida!" : "Fin de la partida", 200, 66, DEPTH_BANNER, 0.62f,
				w == 0 ? COLOR_GOLD : COLOR_RED, C2D_AlignCenter);
		} else {
			gfx_text(RESULT[G->result], 200, 66, DEPTH_BANNER, 0.62f, COLOR_GOLD, C2D_AlignCenter);
		}
		for (int p = 0; p < G->players; p++)
			gfx_textf(200, 90 + p * 15, DEPTH_BANNER, 0.42f, p == 0 ? COLOR_GOLD : COLOR_CREAM, C2D_AlignCenter,
				"%s%s: %d bazas  %+d  → %d", who(p), p == G->hombre ? " (hombre)" : p == G->partner ? " (compañero)" : "",
				G->tricks[p], G->gain[p], G->score[p]);
	}
}

static void ts_draw_bottom(void)
{
	gfx_background(BG_TABLE_BOTTOM);
	table_draw_message(&s_table);
	button_draw(&s_btn_menu, false);
	int n = G->hand_n[0];
	bool mine = G->current == 0;
	if (mine && G->phase == TS_TRUMP) {
		bool king_step = s_pick_trump >= 0;
		gfx_text(king_step ? "¿Qué rey llamas? Quien lo tenga va contigo" : "Elige el triunfo", BOTTOM_W / 2, 28, 0,
			0.47f, COLOR_GOLD, C2D_AlignCenter);
		hand_draw(G->hands[0], n, 0.45f, NULL, -1, NULL);
		for (int i = 0; i < (king_step ? NUM_SUITS + 1 : NUM_SUITS); i++) {
			if (i == NUM_SUITS && !s_btn_suit[i].enabled)
				continue;
			button_draw(&s_btn_suit[i], !king_step || s_btn_suit[i].enabled);
			if (i < NUM_SUITS)
				gfx_sprite_ex(SPR_SUIT_FIRST + i, s_btn_suit[i].x + 18, s_btn_suit[i].y + 14, 0.45f, 0.45f, 0.0f, 0, NULL);
		}
		return;
	}
	bool playable[TS_MAX_HAND];
	for (int i = 0; i < n; i++)
		playable[i] = !(mine && G->phase == TS_PLAYING) || ts_can_play(G, 0, i);
	if (mine && G->phase == TS_BIDDING)
		gfx_text(G->bid ? "¿Subes la puja o pasas?" : "¿Quieres ser el hombre?", BOTTOM_W / 2, 28, 0, 0.47f, COLOR_GOLD,
			C2D_AlignCenter);
	else if (mine && G->phase == TS_DISCARD)
		gfx_textf(BOTTOM_W / 2, 28, 0, 0.45f, COLOR_GOLD, C2D_AlignCenter, "Toca las que cambias (quedan %d en el monte)",
			G->talon_n);
	else if (mine && G->phase == TS_PLAYING)
		gfx_text("¡Tu turno!", BOTTOM_W / 2, 28, 0, 0.5f, COLOR_GOLD, C2D_AlignCenter);
	hand_draw(G->hands[0], n, HAND_SC, G->phase == TS_DISCARD ? s_sel : NULL,
		mine && (G->phase == TS_PLAYING || G->phase == TS_DISCARD) ? s_cursor : -1, playable);
	if (G->phase == TS_BIDDING) {
		if (mine)
			for (int i = 0; i < 4; i++)
				if (!(G->players == 4 && i == TS_VUELTA))
					button_draw(&s_btn_bid[i], s_btn_bid[i].enabled);
		return;
	}
	if (G->phase != TS_TRICK_DONE && G->phase != TS_TRUMP)
		button_draw(&s_btn_main, s_btn_main.enabled);
	if (G->phase >= TS_HAND_OVER)
		button_draw(&s_btn_alt, s_btn_alt.enabled);
}

/* Salir a medias desde la pausa: la próxima vez se empieza de cero */
static void scene_leave(void)
{
	s_started[s_v] = false;
}

const Scene scene_tresillo = { tresillo_enter, ts_update, ts_draw_top, ts_draw_bottom, scene_leave };
const Scene scene_cuatrillo = { cuatrillo_enter, ts_update, ts_draw_top, ts_draw_bottom, scene_leave };
