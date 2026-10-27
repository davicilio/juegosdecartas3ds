/* Mesa de la escoba: el jugador contra Pepe (CPU). */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/escoba.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/flyers.h"
#include "ui/gfx.h"
#include "ui/mii.h"
#include "ui/widgets.h"

#define RIVAL_NAME   "Pepe"
#define RIVAL_Y      36.0f
#define RIVAL_SCALE  0.55f
#define TABLE_SCALE  0.6f
#define DECK_X       46.0f
#define DECK_Y       110.0f
#define PILE_X       354.0f
#define PILE_RIVAL_Y 62.0f
#define PILE_MINE_Y  176.0f
#define HAND_SCALE   1.0f
#define HAND_Y       100.0f
#define SWEEP_TIME   0.6f

static Escoba s_game;
static Mood s_rival_mood, s_my_mood;
static bool s_started;

static VisCard s_hand[DECK_SIZE];  /* mano del jugador, por card_index */
static VisCard s_table[DECK_SIZE]; /* cartas de la mesa, por card_index */
static bool s_sweeping[DECK_SIZE]; /* recogidas, esperando a irse al montón */
static VisCard s_rival[ES_HAND_SIZE];
static int s_rival_shown;
static int s_seen_turns;
static int s_seen_round;
static int s_seen_deals;
static float s_sweep_timer;
static int s_sweep_player;
static int s_selected;
static int s_option;
static float s_cpu_timer;
static float s_result_time;
static float s_escoba_time; /* cartel de "¡Escoba!" */
static char s_message[96];
static float s_message_flash;

static uint32_t s_options[ES_MAX_OPTIONS];
static int s_option_count;

static Button s_btn_play, s_btn_prev, s_btn_next, s_btn_menu;

static void set_message(const char* msg)
{
	snprintf(s_message, sizeof s_message, "%s", msg);
	s_message_flash = 1.0f;
}

static float pile_y(int player)
{
	return player == 0 ? PILE_MINE_Y : PILE_RIVAL_Y;
}

static float rival_slot_x(int i, int n)
{
	return 200.0f + (i - (n - 1) / 2.0f) * 28.0f;
}

/* Posición del hueco i de la mesa (hasta seis cartas por fila). */
static void table_pos(int i, int n, float* x, float* y)
{
	int per_row = n > 12 ? (n + 1) / 2 : 6;
	int rows = (n + per_row - 1) / per_row;
	int row = i / per_row;
	int in_row = row == rows - 1 ? n - row * per_row : per_row;
	float step = per_row > 6 ? 200.0f / (per_row - 1) : 40.0f;
	int col = i % per_row;
	*x = 200.0f + (col - (in_row - 1) / 2.0f) * step;
	*y = rows == 1 ? 132.0f : 102.0f + row * 64.0f;
}

static void refresh_options(void)
{
	s_option_count = 0;
	if (s_game.hand_count[0] > 0 && s_selected < s_game.hand_count[0])
		s_option_count = es_find_captures(&s_game, s_game.hands[0][s_selected], s_options, ES_MAX_OPTIONS);
	if (s_option >= s_option_count)
		s_option = 0;
}

static void reset_visuals(void)
{
	memset(s_hand, 0, sizeof s_hand);
	memset(s_table, 0, sizeof s_table);
	memset(s_sweeping, 0, sizeof s_sweeping);
	memset(s_rival, 0, sizeof s_rival);
	flyers_clear();
	s_rival_shown = 0;
	s_seen_turns = 0;
	s_seen_round = s_game.round;
	s_seen_deals = 0;
	s_sweep_timer = 0.0f;
	s_selected = 0;
	s_option = 0;
	s_cpu_timer = 0.0f;
	s_result_time = 0.0f;
	s_escoba_time = 0.0f;
}

static void announce_round(void)
{
	if (s_game.deal_escobas > 0) {
		set_message(s_game.dealer == 0 ? "¡Escoba al repartir!" : RIVAL_NAME " hace escoba al repartir");
		s_escoba_time = 1.5f;
	} else {
		set_message(s_game.current == 0 ? "Nueva ronda: sales tú" : "Nueva ronda: sale " RIVAL_NAME);
	}
}

static void es_scene_enter(void)
{
	if (!s_started) {
		es_init(&s_game, (uint32_t)time(NULL) ^ 0xE5C0BA);
		s_started = true;
		reset_visuals();
		announce_round();
	}
	button_set(&s_btn_menu, 258, 10, 54, 20, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
}

static bool busy(void)
{
	if (flyers_busy() || s_sweep_timer > 0.0f)
		return true;
	for (int i = 0; i < DECK_SIZE; i++) {
		if (vis_moving(&s_table[i]))
			return true;
		if (s_hand[i].active && s_hand[i].delay > 0.0f)
			return true;
	}
	for (int i = 0; i < s_rival_shown; i++)
		if (vis_moving(&s_rival[i]))
			return true;
	return false;
}

/* Reacciona a la última jugada: la carta vuela a la mesa o se lleva cartas. */
static void on_action(void)
{
	int p = s_game.last_player;
	Card c = s_game.last_card;
	VisCard* v = &s_table[card_index(c)];
	float fx = 200.0f, fy = 290.0f;
	if (p == 1 && s_rival_shown > 0) {
		/* Sale del último hueco de la mano del rival. */
		s_rival_shown--;
		fx = s_rival[s_rival_shown].x;
		fy = s_rival[s_rival_shown].y;
	}
	vis_spawn(v, c, fx, fy, p == 0 ? 0.9f : RIVAL_SCALE, p == 0, 0.0f);

	char card[32], buf[96];
	card_describe_article(c, card, sizeof card);
	if (s_game.last_captured_count > 0) {
		/* La carta jugada se pone junto a lo que recoge y todo se va al montón. */
		const VisCard* first = &s_table[card_index(s_game.last_captured[0])];
		vis_target(v, first->tx + 14.0f, first->ty - 10.0f, TABLE_SCALE, true);
		s_sweeping[card_index(c)] = true;
		for (int i = 0; i < s_game.last_captured_count; i++) {
			int id = card_index(s_game.last_captured[i]);
			s_sweeping[id] = true;
		}
		s_sweep_timer = SWEEP_TIME;
		s_sweep_player = p;
		if (s_game.last_escoba) {
			s_escoba_time = 1.5f;
			mood_set(&s_rival_mood, p == 1 ? EXPR_HAPPY : EXPR_SURPRISED, 2.0f);
			mood_set(&s_my_mood, p == 0 ? EXPR_HAPPY : EXPR_SURPRISED, 2.0f);
			audio_play(SFX_WIN);
			snprintf(buf, sizeof buf, "%s", p == 0 ? "¡Escoba!" : RIVAL_NAME " hace escoba");
		} else if (p == 0) {
			snprintf(buf, sizeof buf, "Te llevas %d cartas", s_game.last_captured_count + 1);
		} else {
			snprintf(buf, sizeof buf, RIVAL_NAME " se lleva %d cartas", s_game.last_captured_count + 1);
		}
	} else {
		snprintf(buf, sizeof buf, p == 0 ? "Dejas %s" : RIVAL_NAME " deja %s", card);
	}
	set_message(buf);
}

static void sync_table(void)
{
	bool present[DECK_SIZE] = { false };
	float delay = 0.0f;
	for (int i = 0; i < s_game.table_count; i++) {
		int id = card_index(s_game.table[i]);
		present[id] = true;
		VisCard* v = &s_table[id];
		if (!v->active) {
			vis_spawn(v, s_game.table[i], DECK_X, DECK_Y, TABLE_SCALE, false, delay);
			delay += 0.12f;
		}
		float x, y;
		table_pos(i, s_game.table_count, &x, &y);
		bool highlighted = false;
		if (s_game.phase == ES_PHASE_PLAYING && s_game.current == 0 && s_option_count > 0)
			highlighted = (s_options[s_option] >> i) & 1u;
		vis_target(v, x, y - (highlighted ? 8.0f : 0.0f), TABLE_SCALE, true);
		v->tangle = ((id * 37) % 9 - 4) * 0.015f;
	}
	/* Cartas que ya no están en la mesa: recogidas. Esperan un momento y se
	 * van al montón de quien las ha ganado. */
	for (int id = 0; id < DECK_SIZE; id++) {
		if (!s_table[id].active || present[id])
			continue;
		if (!s_sweeping[id]) {
			/* Final de ronda: lo que queda es del último que recogió. */
			s_sweeping[id] = true;
			s_sweep_player = s_game.last_capturer;
			if (s_sweep_timer <= 0.0f)
				s_sweep_timer = SWEEP_TIME;
		}
	}
}

static void finish_sweep(void)
{
	int k = 0;
	for (int id = 0; id < DECK_SIZE; id++) {
		if (!s_sweeping[id])
			continue;
		s_sweeping[id] = false;
		if (s_table[id].active)
			flyers_add(&s_table[id], PILE_X, pile_y(s_sweep_player), (k++) * 0.03f, false, FLY_TOP);
		s_table[id].active = false;
	}
	if (k > 0)
		audio_play(SFX_CARD);
}

static void sync_rival(void)
{
	int n = s_game.hand_count[1];
	float delay = 0.0f;
	for (int i = s_rival_shown; i < n; i++) {
		vis_spawn(&s_rival[i], card_make(SUIT_OROS, 1), DECK_X, DECK_Y, RIVAL_SCALE, false, 0.3f + delay);
		delay += 0.12f;
	}
	s_rival_shown = n;
	for (int i = 0; i < n; i++) {
		vis_target(&s_rival[i], rival_slot_x(i, n), RIVAL_Y, RIVAL_SCALE, false);
		s_rival[i].tangle = (i - (n - 1) / 2.0f) * 0.08f;
	}
}

static void sync_hand(void)
{
	const int n = s_game.hand_count[0];
	bool present[DECK_SIZE] = { false };
	bool my_turn = s_game.phase == ES_PHASE_PLAYING && s_game.current == 0;
	float delay = 0.0f;

	for (int i = 0; i < n; i++) {
		Card c = s_game.hands[0][i];
		int id = card_index(c);
		VisCard* v = &s_hand[id];
		present[id] = true;
		if (!v->active) {
			VisCard tmp;
			vis_spawn(&tmp, c, DECK_X, DECK_Y, 0.75f, false, delay);
			flyers_add(&tmp, 200.0f, 290.0f, delay, false, FLY_TOP);
			vis_spawn(v, c, BOTTOM_W / 2.0f, -60.0f, HAND_SCALE, false, delay + 0.3f);
			delay += 0.15f;
		}
		bool selected = my_turn && i == s_selected;
		vis_target(v, BOTTOM_W / 2.0f + (i - (n - 1) / 2.0f) * 78.0f, HAND_Y - (selected ? 12.0f : 0.0f),
			HAND_SCALE, true);
	}
	for (int id = 0; id < DECK_SIZE; id++)
		if (s_hand[id].active && !present[id]) {
			flyers_add(&s_hand[id], s_hand[id].x, -80.0f, 0.0f, true, FLY_BOTTOM);
			s_hand[id].active = false;
		}
}

static int touched_card(const Input* in)
{
	if (!in->touch_down)
		return -1;
	for (int i = 0; i < s_game.hand_count[0]; i++) {
		const VisCard* v = &s_hand[card_index(s_game.hands[0][i])];
		float w = CARD_W * HAND_SCALE / 2.0f, h = CARD_H * HAND_SCALE / 2.0f;
		if (in->tx >= v->x - w && in->tx <= v->x + w && in->ty >= v->y - h && in->ty <= v->y + h)
			return i;
	}
	return -1;
}

static void human_play(void)
{
	uint32_t mask = s_option_count > 0 ? s_options[s_option] : 0;
	if (!es_play(&s_game, s_selected, mask)) {
		audio_play(SFX_ERROR);
		return;
	}
	s_selected = 0;
	s_option = 0;
}

static void change_option(int delta)
{
	if (s_option_count < 2)
		return;
	s_option = (s_option + delta + s_option_count) % s_option_count;
	audio_play(SFX_SELECT);
}

static SceneId es_scene_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;

	bool is_busy = busy();
	const float y = 205, h = 28;
	refresh_options();

	switch (s_game.phase) {
	case ES_PHASE_PLAYING:
		button_set(&s_btn_prev, 12, y, 56, h, GLYPH_L, C2D_Color32(60, 90, 160, 255), KEY_L);
		button_set(&s_btn_play, 80, y, 160, h, GLYPH_A " Jugar", C2D_Color32(40, 140, 60, 255), KEY_A);
		button_set(&s_btn_next, 252, y, 56, h, GLYPH_R, C2D_Color32(60, 90, 160, 255), KEY_R);
		if (s_game.current == 0) {
			s_btn_play.enabled = !is_busy;
			s_btn_prev.enabled = s_btn_next.enabled = !is_busy && s_option_count > 1;
			if (is_busy)
				break;
			int n = s_game.hand_count[0];
			if (n == 0)
				break;
			if (in->repeat & KEY_LEFT) {
				s_selected = (s_selected + n - 1) % n;
				s_option = 0;
				audio_play(SFX_SELECT);
			}
			if (in->repeat & KEY_RIGHT) {
				s_selected = (s_selected + 1) % n;
				s_option = 0;
				audio_play(SFX_SELECT);
			}
			if (in->repeat & (KEY_UP | KEY_DOWN))
				change_option((in->repeat & KEY_UP) ? -1 : 1);
			int t = touched_card(in);
			if (t >= 0) {
				/* Primer toque: elegir. Segundo toque sobre la misma carta: jugar. */
				if (t == s_selected) {
					human_play();
				} else {
					s_selected = t;
					s_option = 0;
					audio_play(SFX_SELECT);
				}
			} else if (button_update(&s_btn_play, in)) {
				human_play();
			} else if (button_update(&s_btn_prev, in)) {
				change_option(-1);
			} else if (button_update(&s_btn_next, in)) {
				change_option(1);
			}
		} else {
			s_btn_play.enabled = s_btn_prev.enabled = s_btn_next.enabled = false;
			s_cpu_timer += dt;
			if (!is_busy && s_cpu_timer > 0.8f) {
				s_cpu_timer = 0.0f;
				uint32_t mask = 0;
				int idx = es_ai_choose(&s_game, 1, &mask);
				es_play(&s_game, idx, mask);
			}
		}
		break;
	case ES_PHASE_ROUND_OVER:
	case ES_PHASE_GAME_OVER: {
		bool over = s_game.phase == ES_PHASE_GAME_OVER;
		button_set(&s_btn_play, over ? 12 : 60, y, over ? 144 : 200, h,
			over ? GLYPH_A " Otra partida" : GLYPH_A " Siguiente ronda", C2D_Color32(40, 140, 60, 255), KEY_A);
		button_set(&s_btn_next, 164, y, 144, h, GLYPH_B " Menú", C2D_Color32(190, 60, 50, 255), KEY_B);
		s_btn_play.enabled = s_btn_next.enabled = !is_busy && s_result_time > 0.6f;
		if (button_update(&s_btn_play, in)) {
			if (over)
				es_init(&s_game, (uint32_t)time(NULL));
			else
				es_next_round(&s_game);
		} else if (over && button_update(&s_btn_next, in)) {
			return SCENE_MENU;
		}
		break;
	}
	}

	/* Cambios en el estado del juego -> animaciones */
	if (s_game.round != s_seen_round) {
		reset_visuals();
		announce_round();
	}
	if (s_game.turns != s_seen_turns) {
		s_seen_turns = s_game.turns;
		on_action();
		s_cpu_timer = 0.0f;
		s_result_time = 0.0f;
	}
	if (s_game.deals != s_seen_deals) {
		if (s_seen_deals > 0 && s_game.deals > 1)
			set_message(s_game.deck.count > 0 ? "Se reparten otras tres cartas" : "Última mano");
		s_seen_deals = s_game.deals;
	}

	if (s_sweep_timer > 0.0f) {
		s_sweep_timer -= dt;
		if (s_sweep_timer <= 0.0f)
			finish_sweep();
	}
	if (s_game.phase != ES_PHASE_PLAYING && !busy()) {
		if (s_result_time == 0.0f) {
			int w = s_game.phase == ES_PHASE_GAME_OVER
				? es_winner(&s_game)
				: (s_game.round_score[0].total >= s_game.round_score[1].total ? 0 : 1);
			audio_play(w == 0 ? SFX_WIN : SFX_LOSE);
			mood_set(&s_rival_mood, w == 1 ? EXPR_HAPPY : EXPR_SAD, 4.0f);
			mood_set(&s_my_mood, w == 0 ? EXPR_HAPPY : EXPR_SAD, 4.0f);
			if (s_game.phase == ES_PHASE_GAME_OVER)
				profile_record_game(GAME_ESCOBA, w == 0);
		}
		s_result_time += dt;
	}

	refresh_options();
	sync_table();
	sync_rival();
	sync_hand();
	for (int i = 0; i < DECK_SIZE; i++) {
		vis_update(&s_hand[i], dt);
		vis_update(&s_table[i], dt);
	}
	for (int i = 0; i < s_rival_shown; i++)
		vis_update(&s_rival[i], dt);
	flyers_update(dt);
	if (s_escoba_time > 0.0f)
		s_escoba_time -= dt;
	mood_update(&s_rival_mood, dt);
	mood_update(&s_my_mood, dt);
	if (s_message_flash > 0.0f)
		s_message_flash -= dt * 2.0f;
	return SCENE_ESCOBA;
}

static void draw_pile(int player)
{
	int cards = s_game.won_count[player];
	float y = pile_y(player);
	/* Las escobas se marcan con cartas boca arriba que asoman bajo el montón. */
	for (int i = 0; i < s_game.escobas[player] && i < 4; i++)
		gfx_card(card_make(SUIT_OROS, 1), PILE_X - 16 - i * 3, y + 6 - i * 3, 0.45f, 1.0f, (float)M_PI / 2.0f,
			DEPTH_CARDS - 0.2f, 0.0f, false);
	if (cards > 0) {
		int layers = cards / 4 + 1;
		if (layers > 6)
			layers = 6;
		for (int i = 0; i < layers; i++)
			gfx_card(card_make(SUIT_OROS, 1), PILE_X - i * 0.5f, y - i * 0.8f, 0.55f, 0.0f,
				0.04f * (i % 2 ? 1 : -1), DEPTH_CARDS + i * 0.1f, 0.0f, i == 0);
	}
	gfx_textf(PILE_X, y + 28, DEPTH_UI, 0.4f, COLOR_CREAM, C2D_AlignCenter, "%d cartas", cards);
	if (s_game.escobas[player] > 0)
		gfx_textf(PILE_X, y + 40, DEPTH_UI, 0.4f, COLOR_GOLD, C2D_AlignCenter, "%d escoba%s", s_game.escobas[player],
			s_game.escobas[player] > 1 ? "s" : "");
}

static void draw_scoreboard(void)
{
	/* Dos capas de cartel: el marcador ha de leerse bien sobre la mesa. */
	banner_draw(s_result_time, 200, 122, 340, 196);
	if (!banner_draw(s_result_time, 200, 122, 340, 196))
		return;
	const EsRoundScore* a = &s_game.round_score[0];
	const EsRoundScore* b = &s_game.round_score[1];
	bool over = s_game.phase == ES_PHASE_GAME_OVER;
	float d = DEPTH_BANNER;

	if (over) {
		int w = es_winner(&s_game);
		gfx_text(w == 0 ? "¡Ganas la partida!" : "Gana " RIVAL_NAME, 200, 28, d, 0.7f, w == 0 ? COLOR_GOLD : COLOR_RED,
			C2D_AlignCenter);
	} else {
		gfx_textf(200, 28, d, 0.65f, COLOR_GOLD, C2D_AlignCenter, "Fin de la ronda %d", s_game.round);
	}

	const float x0 = 50, xa = 262, xb = 326;
	float y = 58;
	gfx_text("Tú", xa, y, d, 0.45f, COLOR_CREAM, C2D_AlignCenter);
	gfx_text(RIVAL_NAME, xb, y, d, 0.45f, COLOR_CREAM, C2D_AlignCenter);
	y += 18;

	char va[16], vb[16];
#define ROW(label, sa, sb, pa, pb)                                                                     \
	do {                                                                                               \
		gfx_text(label, x0, y, d, 0.45f, COLOR_CREAM, C2D_AlignLeft);                                  \
		gfx_text(sa, xa, y, d, 0.45f, (pa) ? COLOR_GOLD : COLOR_WHITE, C2D_AlignCenter);               \
		gfx_text(sb, xb, y, d, 0.45f, (pb) ? COLOR_GOLD : COLOR_WHITE, C2D_AlignCenter);               \
		y += 16;                                                                                       \
	} while (0)

	snprintf(va, sizeof va, "%d", a->escobas);
	snprintf(vb, sizeof vb, "%d", b->escobas);
	ROW("Escobas", va, vb, a->escobas > 0, b->escobas > 0);
	snprintf(va, sizeof va, "%d", a->cards);
	snprintf(vb, sizeof vb, "%d", b->cards);
	ROW("Cartas", va, vb, a->pt_cards, b->pt_cards);
	snprintf(va, sizeof va, "%d", a->oros);
	snprintf(vb, sizeof vb, "%d", b->oros);
	ROW("Oros", va, vb, a->pt_oros, b->pt_oros);
	snprintf(va, sizeof va, "%d", a->sietes);
	snprintf(vb, sizeof vb, "%d", b->sietes);
	ROW("Sietes", va, vb, a->pt_sietes, b->pt_sietes);
	ROW("Siete de oros", a->siete_oros ? "Sí" : "-", b->siete_oros ? "Sí" : "-", a->pt_siete_oros,
		b->pt_siete_oros);
#undef ROW

	gfx_rect(x0, y + 2, 300, 1, d, C2D_Color32(255, 255, 255, 120));
	y += 6;
	gfx_text("Puntos de la ronda", x0, y, d, 0.45f, COLOR_CREAM, C2D_AlignLeft);
	gfx_textf(xa, y, d, 0.5f, COLOR_GOLD, C2D_AlignCenter, "+%d", a->total);
	gfx_textf(xb, y, d, 0.5f, COLOR_GOLD, C2D_AlignCenter, "+%d", b->total);
	y += 18;
	gfx_textf(x0, y, d, 0.45f, COLOR_CREAM, C2D_AlignLeft, "Total (a %d)", ES_GAME_POINTS);
	gfx_textf(xa, y, d, 0.6f, COLOR_WHITE, C2D_AlignCenter, "%d", s_game.score[0]);
	gfx_textf(xb, y, d, 0.6f, COLOR_WHITE, C2D_AlignCenter, "%d", s_game.score[1]);
}

static void es_scene_draw_top(void)
{
	gfx_background(BG_TABLE_TOP);

	/* Marcador de la partida */
	gfx_panel(14, 14, 84, 52, DEPTH_UI, C2D_Color32(10, 50, 25, 210), C2D_Color32(20, 60, 30, 255));
	gfx_textf(56, 17, DEPTH_UI, 0.4f, COLOR_CREAM, C2D_AlignCenter, "A %d puntos", ES_GAME_POINTS);
	mii_draw(&profile_get()->avatar, 28, 40, 6, DEPTH_UI, mood_expr(&s_my_mood), false);
	mii_draw(cpu_avatar(CPU_PEPE), 28, 54, 6, DEPTH_UI, mood_expr(&s_rival_mood), false);
	gfx_textf(40, 33, DEPTH_UI, 0.45f, COLOR_WHITE, C2D_AlignLeft, "Tú %d", s_game.score[0]);
	gfx_textf(40, 47, DEPTH_UI, 0.45f, COLOR_WHITE, C2D_AlignLeft, RIVAL_NAME " %d", s_game.score[1]);

	/* Pepe, junto a sus cartas */
	mii_draw(cpu_avatar(CPU_PEPE), 132, 34, 16, DEPTH_UI, mood_expr(&s_rival_mood), false);

	/* Mazo */
	if (s_game.deck.count > 0) {
		int layers = (s_game.deck.count + 3) / 4;
		for (int i = 0; i < layers; i++)
			gfx_card(card_make(SUIT_OROS, 1), DECK_X - i * 0.5f, DECK_Y - i * 0.7f, 0.75f, 0.0f, 0.0f,
				DEPTH_CARDS + i * 0.08f, 0.0f, i == 0);
		gfx_textf(DECK_X, DECK_Y + 42, DEPTH_UI, 0.42f, COLOR_CREAM, C2D_AlignCenter, "Mazo: %d", s_game.deck.count);
	}

	draw_pile(1);
	draw_pile(0);

	for (int i = 0; i < s_rival_shown; i++)
		vis_draw(&s_rival[i]);
	/* Mesa: primero las que no están resaltadas */
	bool my_turn = s_game.phase == ES_PHASE_PLAYING && s_game.current == 0 && !busy();
	for (int i = 0; i < s_game.table_count; i++) {
		bool hl = my_turn && s_option_count > 0 && ((s_options[s_option] >> i) & 1u);
		const VisCard* v = &s_table[card_index(s_game.table[i])];
		if (hl) {
			float pulse = 0.7f + 0.3f * sinf(gfx_time() * 6.0f);
			C2D_ImageTint tint;
			C2D_AlphaImageTint(&tint, pulse);
			gfx_sprite_ex(SPR_CARD_GLOW, v->x, v->y, TABLE_SCALE, TABLE_SCALE, 0.0f, DEPTH_CARDS, &tint);
		}
		vis_draw(v);
	}
	for (int id = 0; id < DECK_SIZE; id++)
		if (s_sweeping[id])
			vis_draw(&s_table[id]);
	flyers_draw(FLY_TOP);

	if (s_escoba_time > 0.0f) {
		float t = 1.5f - s_escoba_time;
		float s = ease_out_back(fminf(1.0f, t / 0.3f));
		float alpha = fminf(1.0f, s_escoba_time / 0.3f);
		u32 col = C2D_Color32(255, 210, 80, (u8)(255 * alpha));
		gfx_text("¡ESCOBA!", 200, 112 - 22 * s, DEPTH_BANNER + 1.0f, 1.4f * s, col, C2D_AlignCenter);
	}

	if (s_game.phase != ES_PHASE_PLAYING && !busy())
		draw_scoreboard();
}

static void es_scene_draw_bottom(void)
{
	gfx_background(BG_TABLE_BOTTOM);

	gfx_text(s_message, 14, 12, 0, 0.5f, s_message_flash > 0.0f ? COLOR_GOLD : COLOR_CREAM, C2D_AlignLeft);
	button_draw(&s_btn_menu, false);

	bool my_turn = s_game.phase == ES_PHASE_PLAYING && s_game.current == 0;
	if (my_turn)
		gfx_text("¡Tu turno!", BOTTOM_W / 2, 30, 0, 0.55f, COLOR_GOLD, C2D_AlignCenter);
	else if (s_game.phase == ES_PHASE_PLAYING)
		gfx_text("Turno de " RIVAL_NAME "...", BOTTOM_W / 2, 30, 0, 0.5f, COLOR_CREAM, C2D_AlignCenter);

	if (my_turn && s_game.hand_count[0] > 0 && !busy()) {
		const VisCard* v = &s_hand[card_index(s_game.hands[0][s_selected])];
		float pulse = 0.75f + 0.25f * sinf(gfx_time() * 6.0f);
		C2D_ImageTint tint;
		C2D_AlphaImageTint(&tint, pulse);
		gfx_sprite_ex(SPR_CARD_GLOW, v->x, v->y, HAND_SCALE, HAND_SCALE, 0.0f, 0, &tint);
	}
	for (int i = 0; i < s_game.hand_count[0]; i++)
		vis_draw(&s_hand[card_index(s_game.hands[0][i])]);
	flyers_draw(FLY_BOTTOM);

	if (my_turn && s_game.hand_count[0] > 0) {
		Card c = s_game.hands[0][s_selected];
		char line[96];
		if (s_option_count == 0) {
			snprintf(line, sizeof line, "No suma 15: la carta se queda en la mesa");
		} else {
			/* "Te llevas 7 + 5 + 3 = 15" */
			int len = snprintf(line, sizeof line, "Te llevas %d", es_card_value(c));
			for (int i = 0; i < s_game.table_count && len < (int)sizeof line - 8; i++)
				if ((s_options[s_option] >> i) & 1u)
					len += snprintf(line + len, sizeof line - len, " + %d", es_card_value(s_game.table[i]));
			snprintf(line + len, sizeof line - len, " = 15");
		}
		gfx_text(line, BOTTOM_W / 2, 156, 0, 0.5f, s_option_count ? COLOR_GOLD : COLOR_CREAM, C2D_AlignCenter);
		if (s_option_count > 1)
			gfx_textf(BOTTOM_W / 2, 176, 0, 0.42f, C2D_Color32(200, 230, 200, 255), C2D_AlignCenter,
				"Opción %d de %d: cámbiala con " GLYPH_L "/" GLYPH_R, s_option + 1, s_option_count);
		else
			gfx_text("Sota 8 · Caballo 9 · Rey 10", BOTTOM_W / 2, 176, 0, 0.42f, C2D_Color32(200, 230, 200, 255),
				C2D_AlignCenter);
	}

	if (s_game.phase == ES_PHASE_PLAYING) {
		button_draw(&s_btn_prev, false);
		button_draw(&s_btn_play, s_btn_play.enabled);
		button_draw(&s_btn_next, false);
	} else {
		button_draw(&s_btn_play, s_btn_play.enabled);
		if (s_game.phase == ES_PHASE_GAME_OVER)
			button_draw(&s_btn_next, false);
	}
}

const Scene scene_escoba = { es_scene_enter, es_scene_update, es_scene_draw_top, es_scene_draw_bottom };
