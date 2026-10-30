/* Mesa del mus: tú y Lola contra Paco y Manolo. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/mus.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/flyers.h"
#include "ui/gfx.h"
#include "ui/mii.h"
#include "ui/widgets.h"

#define HAND_SCALE 1.0f
#define HAND_Y     104.0f
#define HAND_STEP  70.0f
#define MIN_BET    2
#define MAX_BET    10

/* Posición de las cartas de cada jugador en la pantalla superior */
static const float SEAT_X[MUS_PLAYERS] = { 200, 352, 200, 48 };
static const float SEAT_Y[MUS_PLAYERS] = { 290, 98, 36, 98 };

/* Mii de cada asiento (el 0 eres tú, con tu perfil) */
static const CpuId MIIS[MUS_PLAYERS] = { CPU_PACO, CPU_PACO, CPU_LOLA, CPU_MANOLO };

static const char* seat_name(int p)
{
	return p == 0 ? "Tú" : cpu_name(MIIS[p]);
}

static Mus s_game;
static bool s_started;
static Mood s_mood[MUS_PLAYERS];
/* Señas: cuándo la hará cada jugador de la CPU (-1 = no la hará) */
static float s_sena_timer[MUS_PLAYERS];
static int s_sena_key = -1;
static MusSena s_lola_sena;
static bool s_senas_open;
static int s_senas_cursor;
static Button s_btn_senas;
static Rng s_rng;

static VisCard s_hand[DECK_SIZE]; /* tus cartas, por card_index */
static unsigned s_marked;         /* cartas elegidas para descartar */
static int s_cursor;
static int s_amount = MIN_BET;
static int s_seen_actions;
static int s_seen_lance;
static MusPhase s_seen_phase;
static float s_cpu_timer;
static float s_result_time;
static float s_ordago_time;
static char s_bubble[MUS_PLAYERS][24];
static float s_bubble_time[MUS_PLAYERS];
static char s_message[96];
static float s_message_flash;

static Button s_btn[4], s_btn_menu;
static int s_btn_count;

static void set_message(const char* msg)
{
	snprintf(s_message, sizeof s_message, "%s", msg);
	s_message_flash = 1.0f;
}

static void say(int p, const char* text)
{
	snprintf(s_bubble[p], sizeof s_bubble[p], "%.*s", (int)sizeof s_bubble[p] - 1, text);
	s_bubble_time[p] = 1.8f;
}

static const Avatar* seat_avatar(int p)
{
	return p == 0 ? &profile_get()->avatar : cpu_avatar(MIIS[p]);
}

static Expression sena_expr(MusSena s)
{
	switch (s) {
	case MUS_SENA_REYES:
		return EXPR_BITE_LIP;
	case MUS_SENA_ASES:
		return EXPR_TONGUE;
	case MUS_SENA_MEDIAS:
		return EXPR_TWIST;
	case MUS_SENA_DUPLES:
		return EXPR_BROWS_UP;
	case MUS_SENA_31:
		return EXPR_WINK;
	case MUS_SENA_CIEGO:
		return EXPR_EYES_CLOSED;
	default:
		return EXPR_NORMAL;
	}
}

/* Al empezar cada vuelta de mus (o después de descartar), los jugadores de
 * la CPU se preparan para hacer su seña en un momento al azar. */
static void schedule_senas(void)
{
	int key = s_game.hands_played * 32 + s_game.mus_rounds;
	if (s_game.phase != MUS_PHASE_MUS || key == s_sena_key)
		return;
	s_sena_key = key;
	s_lola_sena = MUS_SENA_NONE;
	for (int p = 1; p < MUS_PLAYERS; p++)
		s_sena_timer[p] = rng_range(&s_rng, 100) < 85 ? 0.4f + rng_range(&s_rng, 220) / 100.0f : -1.0f;
}

#define SENA_TIME 1.6f

static void show_sena(int p, MusSena sena)
{
	mood_set(&s_mood[p], sena_expr(sena), SENA_TIME);
}

static void update_senas(float dt)
{
	if (s_game.phase > MUS_PHASE_BET)
		return;
	for (int p = 1; p < MUS_PLAYERS; p++) {
		if (s_sena_timer[p] < 0.0f)
			continue;
		s_sena_timer[p] -= dt;
		if (s_sena_timer[p] >= 0.0f)
			continue;
		MusSena sena = mus_best_sena(s_game.hands[p]);
		if (sena == MUS_SENA_NONE)
			continue;
		mus_make_sena(&s_game, p, sena);
		/* El gesto se ve en la cara del personaje: ¡atento a los rivales! */
		show_sena(p, sena);
		if (p == 2) {
			s_lola_sena = sena;
			char buf[64];
			snprintf(buf, sizeof buf, "%s te hace una seña", seat_name(2));
			set_message(buf);
		}
	}
}

static void reset_visuals(void)
{
	memset(s_hand, 0, sizeof s_hand);
	flyers_clear();
	s_marked = 0;
	s_cursor = 0;
	s_cpu_timer = 0.0f;
	s_result_time = 0.0f;
	s_ordago_time = 0.0f;
	s_seen_actions = s_game.actions;
	s_seen_lance = -1;
	s_seen_phase = s_game.phase;
	memset(s_bubble_time, 0, sizeof s_bubble_time);
	char buf[96];
	snprintf(buf, sizeof buf, "Nueva mano. Es mano %s", s_game.mano == 0 ? "tú" : seat_name(s_game.mano));
	set_message(buf);
}

static void mus_scene_enter(void)
{
	if (!s_started) {
		mus_init(&s_game, (uint32_t)time(NULL) ^ 0x3005);
		s_started = true;
		reset_visuals();
	}
	button_set(&s_btn_menu, 258, 10, 54, 20, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
	button_set(&s_btn_senas, 192, 10, 60, 20, "Señas", C2D_Color32(60, 90, 160, 255), KEY_SELECT);
	rng_seed(&s_rng, (uint32_t)time(NULL) ^ 0x5E4A5);
	s_senas_open = false;
}

static bool busy(void)
{
	if (flyers_busy())
		return true;
	for (int i = 0; i < DECK_SIZE; i++)
		if (s_hand[i].active && s_hand[i].delay > 0.0f)
			return true;
	return false;
}

/* ---------------------------------------------------------------------------
 * Reacción a cada acción (bocadillos, mensajes, animaciones)
 * ------------------------------------------------------------------------- */

static void announce_lance(void)
{
	int l = s_game.lance;
	char buf[96];
	const char* name = mus_lance_name(&s_game, l);
	if (l == MUS_LANCE_PARES || (l == MUS_LANCE_JUEGO && !s_game.punto)) {
		/* Cada jugador dice si tiene o no */
		for (int p = 0; p < MUS_PLAYERS; p++)
			say(p, s_game.eligible[p] ? (l == MUS_LANCE_PARES ? "Pares sí" : "Juego sí")
			                          : (l == MUS_LANCE_PARES ? "Pares no" : "Juego no"));
	}
	snprintf(buf, sizeof buf, "A %s", name);
	set_message(buf);
	s_amount = MIN_BET;
}

static void on_action(void)
{
	int p = s_game.last_player;
	char buf[96];
	if (p < 0)
		return;
	switch (s_game.last_action) {
	case MUS_ACT_MUS:
		say(p, "¡Mus!");
		break;
	case MUS_ACT_NO_MUS:
		say(p, "No hay mus");
		snprintf(buf, sizeof buf, "%s corta el mus", seat_name(p));
		set_message(buf);
		audio_play(SFX_CONFIRM);
		break;
	case MUS_ACT_DISCARD:
		snprintf(buf, sizeof buf, "Descarta %d", s_game.last_amount);
		say(p, buf);
		if (p != 0) {
			/* Las cartas van al centro y vuelven otras tantas */
			VisCard v;
			for (int i = 0; i < s_game.last_amount; i++) {
				vis_spawn(&v, card_make(SUIT_OROS, 1), SEAT_X[p], SEAT_Y[p], 0.5f, false, i * 0.08f);
				flyers_add(&v, 200.0f, 140.0f, i * 0.08f, false, FLY_TOP);
				vis_spawn(&v, card_make(SUIT_OROS, 1), 200.0f, 140.0f, 0.5f, false, 0.35f + i * 0.08f);
				flyers_add(&v, SEAT_X[p], SEAT_Y[p], 0.35f + i * 0.08f, false, FLY_TOP);
			}
		}
		break;
	case MUS_ACT_PASO:
		say(p, "Paso");
		break;
	case MUS_ACT_ENVIDO:
		if (s_game.bet_accepted > 0)
			snprintf(buf, sizeof buf, "¡Subo a %d!", s_game.last_amount);
		else
			snprintf(buf, sizeof buf, "¡Envido %d!", s_game.last_amount);
		say(p, buf);
		audio_play(SFX_CHIP);
		break;
	case MUS_ACT_ORDAGO:
		say(p, "¡ÓRDAGO!");
		for (int q = 0; q < MUS_PLAYERS; q++)
			if (q != p)
				mood_set(&s_mood[q], EXPR_SURPRISED, 1.6f);
		s_ordago_time = 1.6f;
		audio_play(SFX_ERROR);
		break;
	case MUS_ACT_QUIERO:
		say(p, "Quiero");
		audio_play(SFX_CHIP);
		break;
	case MUS_ACT_NO_QUIERO:
		say(p, "No quiero");
		break;
	default:
		break;
	}
}

static void sync_hand(void)
{
	bool present[DECK_SIZE] = { false };
	bool discarding = s_game.phase == MUS_PHASE_DISCARD && s_game.current == 0;
	float delay = 0.0f;
	for (int i = 0; i < MUS_HAND_SIZE; i++) {
		Card c = s_game.hands[0][i];
		int id = card_index(c);
		VisCard* v = &s_hand[id];
		present[id] = true;
		if (!v->active) {
			vis_spawn(v, c, BOTTOM_W / 2.0f, -60.0f, HAND_SCALE, false, delay + 0.2f);
			delay += 0.12f;
		}
		bool marked = discarding && (s_marked & (1u << i));
		bool cursor = discarding && i == s_cursor;
		vis_target(v, BOTTOM_W / 2.0f + (i - 1.5f) * HAND_STEP, HAND_Y - (marked ? 12.0f : cursor ? 4.0f : 0.0f),
			HAND_SCALE, true);
		v->dim = marked ? 0.45f : 0.0f;
	}
	for (int id = 0; id < DECK_SIZE; id++)
		if (s_hand[id].active && !present[id]) {
			flyers_add(&s_hand[id], s_hand[id].x, -80.0f, 0.0f, false, FLY_BOTTOM);
			s_hand[id].active = false;
		}
}

static int touched_card(const Input* in)
{
	if (!in->touch_down)
		return -1;
	for (int i = MUS_HAND_SIZE - 1; i >= 0; i--) {
		const VisCard* v = &s_hand[card_index(s_game.hands[0][i])];
		float w = CARD_W * HAND_SCALE / 2.0f, h = CARD_H * HAND_SCALE / 2.0f;
		if (in->tx >= v->x - w && in->tx <= v->x + w && in->ty >= v->y - h && in->ty <= v->y + h)
			return i;
	}
	return -1;
}

/* ---------------------------------------------------------------------------
 * Botones según la fase
 * ------------------------------------------------------------------------- */

static void layout_buttons(bool my_turn, bool is_busy)
{
	const float y = 205, h = 28;
	static char envido_label[32];
	s_btn_count = 0;
	switch (s_game.phase) {
	case MUS_PHASE_MUS:
		button_set(&s_btn[0], 12, y, 144, h, GLYPH_A " ¡Mus!", C2D_Color32(40, 140, 60, 255), KEY_A);
		button_set(&s_btn[1], 164, y, 144, h, GLYPH_B " No hay mus", C2D_Color32(190, 60, 50, 255), KEY_B);
		s_btn_count = 2;
		break;
	case MUS_PHASE_DISCARD:
		button_set(&s_btn[0], 60, y, 200, h, GLYPH_A " Descartar", C2D_Color32(40, 140, 60, 255), KEY_A);
		s_btn_count = 1;
		break;
	case MUS_PHASE_BET:
		snprintf(envido_label, sizeof envido_label, GLYPH_X " %s %d", mus_responding(&s_game) ? "Subo" : "Envido",
			s_amount);
		if (mus_responding(&s_game)) {
			bool ordago = s_game.bet_pending == MUS_ORDAGO;
			button_set(&s_btn[0], 6, y, 74, h, GLYPH_A " Quiero", C2D_Color32(40, 140, 60, 255), KEY_A);
			button_set(&s_btn[1], 84, y, 74, h, GLYPH_B " No", C2D_Color32(190, 60, 50, 255), KEY_B);
			button_set(&s_btn[2], 162, y, 74, h, envido_label, C2D_Color32(60, 90, 160, 255), KEY_X);
			button_set(&s_btn[3], 240, y, 74, h, GLYPH_Y " Órdago", C2D_Color32(120, 30, 120, 255), KEY_Y);
			s_btn[2].enabled = s_btn[3].enabled = !ordago;
			s_btn_count = 4;
		} else {
			button_set(&s_btn[0], 6, y, 100, h, GLYPH_A " Paso", C2D_Color32(40, 140, 60, 255), KEY_A);
			button_set(&s_btn[1], 110, y, 100, h, envido_label, C2D_Color32(60, 90, 160, 255), KEY_X);
			button_set(&s_btn[2], 214, y, 100, h, GLYPH_Y " Órdago", C2D_Color32(120, 30, 120, 255), KEY_Y);
			s_btn_count = 3;
		}
		break;
	case MUS_PHASE_HAND_OVER:
		button_set(&s_btn[0], 60, y, 200, h, GLYPH_A " Siguiente mano", C2D_Color32(40, 140, 60, 255), KEY_A);
		s_btn_count = 1;
		break;
	case MUS_PHASE_GAME_OVER:
		button_set(&s_btn[0], 12, y, 144, h, GLYPH_A " Otra partida", C2D_Color32(40, 140, 60, 255), KEY_A);
		button_set(&s_btn[1], 164, y, 144, h, GLYPH_B " Menú", C2D_Color32(190, 60, 50, 255), KEY_B);
		s_btn_count = 2;
		break;
	}
	bool results = s_game.phase == MUS_PHASE_HAND_OVER || s_game.phase == MUS_PHASE_GAME_OVER;
	for (int i = 0; i < s_btn_count; i++) {
		bool en = s_btn[i].enabled;
		s_btn[i].enabled = en && !is_busy && (results ? s_result_time > 0.8f : my_turn);
	}
}

#define SENA_X(i) (10.0f + ((i) % 2) * 152.0f)
#define SENA_Y(i) (58.0f + ((i) / 2) * 46.0f)
#define SENA_W 148.0f
#define SENA_H 42.0f

static void choose_sena(MusSena sena)
{
	mus_make_sena(&s_game, 0, sena);
	show_sena(0, sena);
	char buf[96];
	snprintf(buf, sizeof buf, "Seña a %s: %s", seat_name(2), mus_sena_meaning(sena));
	set_message(buf);
	audio_play(SFX_CONFIRM);
	s_senas_open = false;
}

/* Panel de señas: devuelve true si se ha quedado abierto */
static bool update_senas_panel(const Input* in)
{
	if (s_game.phase > MUS_PHASE_BET) {
		s_senas_open = false;
		return false;
	}
	if (!s_senas_open) {
		if (button_update(&s_btn_senas, in)) {
			s_senas_open = true;
			s_senas_cursor = 0;
		}
		return s_senas_open;
	}
	const int n = MUS_NUM_SENAS - 1;
	if (in->repeat & KEY_RIGHT)
		s_senas_cursor = (s_senas_cursor + 1) % n;
	if (in->repeat & KEY_LEFT)
		s_senas_cursor = (s_senas_cursor + n - 1) % n;
	if (in->repeat & KEY_DOWN)
		s_senas_cursor = (s_senas_cursor + 2) % n;
	if (in->repeat & KEY_UP)
		s_senas_cursor = (s_senas_cursor + n - 2) % n;
	if (in->down & KEY_A) {
		choose_sena((MusSena)(s_senas_cursor + 1));
		return false;
	}
	for (int i = 0; i < n; i++)
		if (input_touch_in(in, SENA_X(i), SENA_Y(i), SENA_W, SENA_H)) {
			choose_sena((MusSena)(i + 1));
			return false;
		}
	if (in->down & (KEY_B | KEY_SELECT) || input_touch_in(in, 60, 205, 200, 28))
		s_senas_open = false;
	return s_senas_open;
}

static SceneId mus_scene_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;
	bool panel = update_senas_panel(in);

	bool is_busy = busy();
	bool my_turn = s_game.current == 0 && s_game.phase <= MUS_PHASE_BET;
	layout_buttons(my_turn, is_busy);

	if (s_game.phase <= MUS_PHASE_BET && !my_turn) {
		/* Turno de la CPU (incluida tu compañera) */
		s_cpu_timer += dt;
		if (!is_busy && s_cpu_timer > 0.9f) {
			s_cpu_timer = 0.0f;
			int p = s_game.current;
			if (s_game.phase == MUS_PHASE_MUS) {
				mus_say_mus(&s_game, mus_ai_wants_mus(&s_game, p));
			} else if (s_game.phase == MUS_PHASE_DISCARD) {
				mus_discard(&s_game, mus_ai_discard(&s_game, p));
			} else {
				int amount;
				MusAction a = mus_ai_bet(&s_game, p, &amount);
				mus_bet(&s_game, a, amount);
			}
		}
	} else if (my_turn && !is_busy && !panel) {
		switch (s_game.phase) {
		case MUS_PHASE_MUS:
			if (button_update(&s_btn[0], in))
				mus_say_mus(&s_game, true);
			else if (button_update(&s_btn[1], in))
				mus_say_mus(&s_game, false);
			break;
		case MUS_PHASE_DISCARD: {
			if (in->repeat & KEY_LEFT) {
				s_cursor = (s_cursor + MUS_HAND_SIZE - 1) % MUS_HAND_SIZE;
				audio_play(SFX_SELECT);
			}
			if (in->repeat & KEY_RIGHT) {
				s_cursor = (s_cursor + 1) % MUS_HAND_SIZE;
				audio_play(SFX_SELECT);
			}
			int t = touched_card(in);
			if (t >= 0)
				s_cursor = t;
			if (t >= 0 || (in->down & (KEY_X | KEY_UP | KEY_DOWN))) {
				s_marked ^= 1u << s_cursor;
				audio_play(SFX_FLIP);
			}
			s_btn[0].enabled = s_marked != 0;
			if (button_update(&s_btn[0], in)) {
				mus_discard(&s_game, s_marked);
				s_marked = 0;
			}
			break;
		}
		case MUS_PHASE_BET:
			if (in->repeat & (KEY_L | KEY_LEFT)) {
				s_amount = s_amount > MIN_BET ? s_amount - 1 : MIN_BET;
				audio_play(SFX_SELECT);
			}
			if (in->repeat & (KEY_R | KEY_RIGHT)) {
				s_amount = s_amount < MAX_BET ? s_amount + 1 : MAX_BET;
				audio_play(SFX_SELECT);
			}
			if (mus_responding(&s_game)) {
				if (button_update(&s_btn[0], in))
					mus_bet(&s_game, MUS_ACT_QUIERO, 0);
				else if (button_update(&s_btn[1], in))
					mus_bet(&s_game, MUS_ACT_NO_QUIERO, 0);
				else if (button_update(&s_btn[2], in))
					mus_bet(&s_game, MUS_ACT_ENVIDO, s_amount);
				else if (button_update(&s_btn[3], in))
					mus_bet(&s_game, MUS_ACT_ORDAGO, 0);
			} else {
				if (button_update(&s_btn[0], in))
					mus_bet(&s_game, MUS_ACT_PASO, 0);
				else if (button_update(&s_btn[1], in))
					mus_bet(&s_game, MUS_ACT_ENVIDO, s_amount);
				else if (button_update(&s_btn[2], in))
					mus_bet(&s_game, MUS_ACT_ORDAGO, 0);
			}
			break;
		default:
			break;
		}
	} else if (s_game.phase == MUS_PHASE_HAND_OVER) {
		if (button_update(&s_btn[0], in)) {
			mus_next_hand(&s_game);
			reset_visuals();
		}
	} else if (s_game.phase == MUS_PHASE_GAME_OVER) {
		if (button_update(&s_btn[0], in)) {
			mus_init(&s_game, (uint32_t)time(NULL));
			reset_visuals();
		} else if (button_update(&s_btn[1], in)) {
			return SCENE_MENU;
		}
	}

	/* Cambios en el juego -> bocadillos y mensajes */
	if (s_game.actions != s_seen_actions) {
		s_seen_actions = s_game.actions;
		on_action();
		s_cpu_timer = 0.0f;
	}
	if (s_game.phase == MUS_PHASE_BET && s_game.lance != s_seen_lance) {
		s_seen_lance = s_game.lance;
		announce_lance();
	}
	if (s_game.phase != s_seen_phase) {
		if (s_game.phase == MUS_PHASE_DISCARD)
			set_message("¡Mus! Toca cambiar cartas");
		else if (s_game.phase == MUS_PHASE_HAND_OVER || s_game.phase == MUS_PHASE_GAME_OVER) {
			s_result_time = 0.0f;
			bool won = s_game.phase == MUS_PHASE_GAME_OVER ? s_game.winner_team == 0
			                                               : s_game.results[0].points[0] +
			                                                     s_game.results[1].points[0] +
			                                                     s_game.results[2].points[0] +
			                                                     s_game.results[3].points[0] >=
			                                                   s_game.results[0].points[1] +
			                                                     s_game.results[1].points[1] +
			                                                     s_game.results[2].points[1] +
			                                                     s_game.results[3].points[1];
			audio_play(won ? SFX_WIN : SFX_LOSE);
			for (int p = 0; p < MUS_PLAYERS; p++)
				mood_set(&s_mood[p], (mus_team(p) == 0) == won ? EXPR_HAPPY : EXPR_SAD, 3.5f);
			if (s_game.phase == MUS_PHASE_GAME_OVER)
				profile_record_game(GAME_MUS, s_game.winner_team == 0);
			set_message(s_game.phase == MUS_PHASE_GAME_OVER ? "Fin de la partida" : "Se descubren las cartas");
		} else if (s_game.phase == MUS_PHASE_MUS && s_seen_phase == MUS_PHASE_DISCARD) {
			set_message("Cartas nuevas: ¿mus?");
		}
		s_seen_phase = s_game.phase;
	}
	if (s_game.phase >= MUS_PHASE_HAND_OVER && !busy())
		s_result_time += dt;

	sync_hand();
	for (int i = 0; i < DECK_SIZE; i++)
		vis_update(&s_hand[i], dt);
	flyers_update(dt);
	for (int p = 0; p < MUS_PLAYERS; p++) {
		if (s_bubble_time[p] > 0.0f)
			s_bubble_time[p] -= dt;
		mood_update(&s_mood[p], dt);
	}
	schedule_senas();
	update_senas(dt);
	if (s_ordago_time > 0.0f)
		s_ordago_time -= dt;
	if (s_message_flash > 0.0f)
		s_message_flash -= dt * 2.0f;
	return SCENE_MUS;
}

/* ---------------------------------------------------------------------------
 * Dibujo
 * ------------------------------------------------------------------------- */

static bool revealed(void)
{
	return s_game.phase >= MUS_PHASE_HAND_OVER;
}

static void draw_seat(int p)
{
	/* Cartas del jugador: boca abajo, o descubiertas al final */
	float cx = SEAT_X[p], cy = SEAT_Y[p];
	bool up = revealed();
	float scale = up ? 0.5f : 0.45f;
	float step = p == 2 ? (up ? 34.0f : 16.0f) : (up ? 17.0f : 9.0f);
	for (int i = 0; i < MUS_HAND_SIZE; i++)
		gfx_card(s_game.hands[p][i], cx + (i - 1.5f) * step, cy, scale, up ? 1.0f : 0.0f, up ? 0.0f : (i - 1.5f) * 0.1f,
			DEPTH_CARDS + i * 0.05f, 0.0f, i == 0);

	/* Su personaje (las señas se ven en su cara) */
	float ax = p == 2 ? 126.0f : cx, ay = p == 2 ? 36.0f : 50.0f;
	mii_draw(seat_avatar(p), ax, ay, 17, DEPTH_UI + 0.5f, mood_expr(&s_mood[p]), false);

	/* Nombre, turno y mano */
	bool turn = s_game.current == p && s_game.phase <= MUS_PHASE_BET;
	bool out = s_game.phase == MUS_PHASE_BET && !s_game.eligible[p];
	float nx = p == 2 ? cx + 110 : cx, ny = p == 2 ? cy - 12 : cy + 32;
	u32 border = turn ? COLOR_GOLD : mus_team(p) == 0 ? C2D_Color32(60, 120, 200, 255) : C2D_Color32(190, 60, 50, 255);
	gfx_panel(nx - 38, ny, 76, 22, DEPTH_UI, out ? C2D_Color32(40, 40, 40, 200) : C2D_Color32(10, 50, 25, 220), border);
	gfx_text(seat_name(p), nx, ny + 4, DEPTH_UI, 0.45f, out ? C2D_Color32(150, 150, 150, 255) : COLOR_CREAM,
		C2D_AlignCenter);
	if (s_game.mano == p)
		gfx_text("mano", nx + (p == 2 ? 50 : 0), ny + (p == 2 ? 4 : 24), DEPTH_UI, 0.38f, COLOR_GOLD, C2D_AlignCenter);
}

static void draw_bubble(int p)
{
	if (s_bubble_time[p] <= 0.0f)
		return;
	/* Bocadillo junto al nombre de cada jugador */
	float bx = p == 0 ? 200 : p == 2 ? 310 : SEAT_X[p], by = p == 0 ? 214 : p == 2 ? 62 : SEAT_Y[p] + 70;
	float pop = ease_out_back(fminf(1.0f, (1.8f - s_bubble_time[p]) / 0.2f));
	float w = gfx_text_width(s_bubble[p], 0.5f) + 16;
	gfx_rrect(bx - w / 2 * pop, by - 11 * pop, w * pop, 22 * pop, 8, DEPTH_BANNER, C2D_Color32(255, 250, 235, 240));
	if (pop > 0.8f)
		gfx_text(s_bubble[p], bx, by - 8, DEPTH_BANNER, 0.5f, C2D_Color32(120, 20, 20, 255), C2D_AlignCenter);
}

static void lance_status(int l, char* buf, int size, u32* color)
{
	const MusLanceResult* r = &s_game.results[l];
	bool done = s_game.phase >= MUS_PHASE_HAND_OVER;
	*color = COLOR_CREAM;
	if (done && r->kind != MUS_RES_NONE && (r->points[0] || r->points[1])) {
		int team = r->points[0] ? 0 : 1;
		snprintf(buf, size, "+%d %s", r->points[team], team == 0 ? "Nosotros" : "Ellos");
		*color = team == 0 ? C2D_Color32(140, 200, 255, 255) : C2D_Color32(255, 150, 140, 255);
		return;
	}
	switch (r->kind) {
	case MUS_RES_NONE:
		snprintf(buf, size, "Nadie tiene");
		break;
	case MUS_RES_SINGLE:
		snprintf(buf, size, "Solo una pareja");
		break;
	case MUS_RES_PASO:
		snprintf(buf, size, "En paso");
		break;
	case MUS_RES_QUERIDO:
		snprintf(buf, size, "Querido: %d", r->amount);
		break;
	case MUS_RES_NO_QUERIDO:
		snprintf(buf, size, "No querido");
		break;
	case MUS_RES_ORDAGO:
		snprintf(buf, size, "¡Órdago querido!");
		*color = COLOR_GOLD;
		break;
	default:
		if (s_game.phase == MUS_PHASE_BET && s_game.lance == l) {
			*color = COLOR_GOLD;
			if (s_game.bet_pending == MUS_ORDAGO)
				snprintf(buf, size, "¡Órdago!");
			else if (s_game.bet_pending > 0)
				snprintf(buf, size, "Envite: %d", s_game.bet_pending);
			else
				snprintf(buf, size, "Hablando...");
		} else {
			snprintf(buf, size, "-");
		}
		break;
	}
}

static void draw_board(void)
{
	float x = 112, y = 70, w = 176, h = 106;
	gfx_panel(x, y, w, h, DEPTH_UI, C2D_Color32(10, 40, 20, 220), C2D_Color32(200, 160, 60, 255));
	for (int l = 0; l < MUS_NUM_LANCES; l++) {
		float ly = y + 8 + l * 24;
		bool current = s_game.phase == MUS_PHASE_BET && s_game.lance == l;
		if (current)
			gfx_rrect(x + 4, ly - 2, w - 8, 22, 5, DEPTH_UI, C2D_Color32(255, 220, 90, 60));
		gfx_text(mus_lance_name(&s_game, l), x + 10, ly, DEPTH_UI, 0.48f, current ? COLOR_GOLD : COLOR_WHITE,
			C2D_AlignLeft);
		char buf[40];
		u32 col;
		lance_status(l, buf, sizeof buf, &col);
		gfx_text(buf, x + w - 10, ly + 1, DEPTH_UI, 0.42f, col, C2D_AlignRight);
	}
}

static const char* ordago_label(void)
{
	switch (s_game.lance) {
	case MUS_LANCE_GRANDE:
		return "Órdago a la grande";
	case MUS_LANCE_CHICA:
		return "Órdago a la chica";
	case MUS_LANCE_PARES:
		return "Órdago a los pares";
	default:
		return s_game.punto ? "Órdago al punto" : "Órdago al juego";
	}
}

static void mus_scene_draw_top(void)
{
	gfx_background(BG_TABLE_TOP);

	/* Marcador */
	gfx_panel(14, 182, 90, 48, DEPTH_UI, C2D_Color32(10, 50, 25, 210), C2D_Color32(20, 60, 30, 255));
	gfx_textf(59, 184, DEPTH_UI, 0.4f, COLOR_CREAM, C2D_AlignCenter, "A %d piedras", MUS_GAME_POINTS);
	gfx_textf(59, 198, DEPTH_UI, 0.45f, C2D_Color32(140, 200, 255, 255), C2D_AlignCenter, "Nosotros %d",
		s_game.score[0]);
	gfx_textf(59, 212, DEPTH_UI, 0.45f, C2D_Color32(255, 150, 140, 255), C2D_AlignCenter, "Ellos %d", s_game.score[1]);

	/* Tu personaje, abajo: también hace las señas que tú elijas */
	mii_draw(seat_avatar(0), 124, 210, 15, DEPTH_UI + 0.5f, mood_expr(&s_mood[0]), false);

	for (int p = 1; p < MUS_PLAYERS; p++)
		draw_seat(p);
	draw_board();
	flyers_draw(FLY_TOP);

	/* Tu jugada al descubrir */
	if (revealed()) {
		for (int i = 0; i < MUS_HAND_SIZE; i++)
			gfx_card(s_game.hands[0][i], 200 + (i - 1.5f) * 34.0f, 210, 0.5f, 1.0f, 0.0f, DEPTH_CARDS, 0.0f, i == 0);
	}
	for (int p = 0; p < MUS_PLAYERS; p++)
		draw_bubble(p);

	if (s_ordago_time > 0.0f) {
		float t = 1.6f - s_ordago_time;
		float s = ease_out_back(fminf(1.0f, t / 0.3f));
		float alpha = fminf(1.0f, s_ordago_time / 0.3f);
		gfx_text("¡ÓRDAGO!", 200, 100 - 22 * s, DEPTH_BANNER + 1.0f, 1.4f * s,
			C2D_Color32(255, 90, 60, (u8)(255 * alpha)), C2D_AlignCenter);
	}
	if (s_game.phase == MUS_PHASE_GAME_OVER && s_result_time > 0.0f && s_ordago_time <= 0.0f) {
		if (banner_draw(s_result_time, 200, 124, 300, 60)) {
			bool won = s_game.winner_team == 0;
			char who[80];
			snprintf(who, sizeof who, "Ganan %s y %s", seat_name(1), seat_name(3));
			gfx_text(won ? "¡Ganáis la partida!" : who, 200, 100, DEPTH_BANNER, 0.75f,
				won ? COLOR_GOLD : COLOR_RED, C2D_AlignCenter);
			if (s_game.ordago_played)
				gfx_text(ordago_label(), 200, 128, DEPTH_BANNER, 0.5f, COLOR_CREAM, C2D_AlignCenter);
			else
				gfx_textf(200, 128, DEPTH_BANNER, 0.5f, COLOR_CREAM, C2D_AlignCenter, "Nosotros %d - %d Ellos",
					s_game.score[0], s_game.score[1]);
		}
	}
}

static void mus_scene_draw_bottom(void)
{
	gfx_background(BG_TABLE_BOTTOM);

	gfx_text(s_message, 14, 13, 0, 0.45f, s_message_flash > 0.0f ? COLOR_GOLD : COLOR_CREAM, C2D_AlignLeft);
	button_draw(&s_btn_menu, false);

	bool my_turn = s_game.current == 0 && s_game.phase <= MUS_PHASE_BET;
	const char* prompt = NULL;
	if (my_turn) {
		switch (s_game.phase) {
		case MUS_PHASE_MUS:
			prompt = "¿Mus o no hay mus?";
			break;
		case MUS_PHASE_DISCARD:
			prompt = "Toca las cartas que quieres cambiar";
			break;
		default:
			prompt = mus_responding(&s_game) ? "Te han envidado: ¿quieres?" : "¿Pasas o envidas?";
			break;
		}
		gfx_text(prompt, BOTTOM_W / 2, 30, 0, 0.5f, COLOR_GOLD, C2D_AlignCenter);
	} else if (s_game.phase <= MUS_PHASE_BET) {
		gfx_textf(BOTTOM_W / 2, 30, 0, 0.5f, COLOR_CREAM, C2D_AlignCenter, "Habla %s...", seat_name(s_game.current));
	}

	if (s_game.phase == MUS_PHASE_DISCARD && my_turn && !busy()) {
		const VisCard* v = &s_hand[card_index(s_game.hands[0][s_cursor])];
		float pulse = 0.75f + 0.25f * sinf(gfx_time() * 6.0f);
		C2D_ImageTint tint;
		C2D_AlphaImageTint(&tint, pulse);
		gfx_sprite_ex(SPR_CARD_GLOW, v->x, v->y, HAND_SCALE, HAND_SCALE, 0.0f, 0, &tint);
	}
	for (int i = 0; i < MUS_HAND_SIZE; i++)
		vis_draw(&s_hand[card_index(s_game.hands[0][i])]);
	flyers_draw(FLY_BOTTOM);

	/* Tus jugadas, para no tener que contar */
	char pares[40], juego[40], grande[24];
	mus_describe(s_game.hands[0], MUS_LANCE_GRANDE, false, grande, sizeof grande);
	mus_describe(s_game.hands[0], MUS_LANCE_PARES, false, pares, sizeof pares);
	mus_describe(s_game.hands[0], MUS_LANCE_JUEGO, s_game.punto, juego, sizeof juego);
	gfx_textf(BOTTOM_W / 2, 158, 0, 0.45f, COLOR_CREAM, C2D_AlignCenter, "%s  ·  %s  ·  %s", grande, pares, juego);
	if (s_game.phase == MUS_PHASE_BET && my_turn)
		gfx_textf(BOTTOM_W / 2, 177, 0, 0.42f, C2D_Color32(200, 230, 200, 255), C2D_AlignCenter,
			"%s · Cambia el envite con " GLYPH_L "/" GLYPH_R, mus_lance_name(&s_game, s_game.lance));
	else if (s_game.phase == MUS_PHASE_DISCARD && my_turn)
		gfx_text("Marca con " GLYPH_X " o tocando · " GLYPH_A " para cambiarlas", BOTTOM_W / 2, 177, 0, 0.42f,
			C2D_Color32(200, 230, 200, 255), C2D_AlignCenter);
	else
		gfx_textf(BOTTOM_W / 2, 177, 0, 0.42f, C2D_Color32(200, 230, 200, 255), C2D_AlignCenter,
			"Tú y %s contra %s y %s", seat_name(2), seat_name(1), seat_name(3));

	for (int i = 0; i < s_btn_count; i++)
		button_draw(&s_btn[i], i == 0 && s_btn[i].enabled);
	if (s_game.phase <= MUS_PHASE_BET)
		button_draw(&s_btn_senas, false);

	if (!s_senas_open)
		return;
	/* Panel de señas */
	gfx_rect(0, 0, BOTTOM_W, SCREEN_H, 0, C2D_Color32(0, 0, 0, 150));
	gfx_panel(4, 4, BOTTOM_W - 8, SCREEN_H - 8, 0, C2D_Color32(20, 45, 90, 245), COLOR_GOLD);
	gfx_textf(BOTTOM_W / 2, 9, 0, 0.55f, COLOR_GOLD, C2D_AlignCenter, "Señas para %s", seat_name(2));
	char buf[96];
	if (s_lola_sena != MUS_SENA_NONE)
		snprintf(buf, sizeof buf, "%s te dice: %s (%s)", seat_name(2), mus_sena_meaning(s_lola_sena),
			mus_sena_gesture(s_lola_sena));
	else
		snprintf(buf, sizeof buf, "%s aún no te ha hecho ninguna seña", seat_name(2));
	gfx_text(buf, BOTTOM_W / 2, 30, 0, 0.42f, COLOR_CREAM, C2D_AlignCenter);
	for (int i = 0; i < MUS_NUM_SENAS - 1; i++) {
		MusSena sena = (MusSena)(i + 1);
		float x = SENA_X(i), y = SENA_Y(i);
		bool sel = i == s_senas_cursor;
		gfx_rrect(x, y, SENA_W, SENA_H, 7, 0, sel ? C2D_Color32(255, 236, 170, 255) : C2D_Color32(250, 244, 228, 255));
		if (sel)
			gfx_rrect(x, y + SENA_H - 3, SENA_W, 3, 1, 0, COLOR_GOLD);
		mii_draw(seat_avatar(0), x + 20, y + 21, 12, 0, sena_expr(sena), false);
		gfx_text(mus_sena_meaning(sena), x + 38, y + 6, 0, 0.45f, C2D_Color32(90, 15, 20, 255), C2D_AlignLeft);
		gfx_text(mus_sena_gesture(sena), x + 38, y + 23, 0, 0.36f, C2D_Color32(110, 90, 80, 255), C2D_AlignLeft);
	}
	gfx_rrect(60, 205, 200, 28, 8, 0, C2D_Color32(190, 60, 50, 255));
	gfx_text(GLYPH_B " Cerrar", BOTTOM_W / 2, 210, 0, 0.55f, COLOR_WHITE, C2D_AlignCenter);
}

/* Salir a medias desde la pausa: la próxima vez se empieza de cero */
static void scene_leave(void)
{
	s_started = false;
}

const Scene scene_mus = { mus_scene_enter, mus_scene_update, mus_scene_draw_top, mus_scene_draw_bottom, scene_leave };
