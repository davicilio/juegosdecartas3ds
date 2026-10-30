/* Mesa del chinchón: el jugador contra Lucía (CPU). */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/chinchon.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/flyers.h"
#include "ui/gfx.h"
#include "ui/mii.h"
#include "ui/widgets.h"

#define RIVAL CPU_LUCIA /* su nombre es el del Mii que ocupe el puesto */
#define RIVAL_Y     38.0f
#define RIVAL_SCALE 0.55f
#define DECK_X      158.0f
#define DECK_Y      126.0f
#define DISCARD_X   236.0f
#define DISCARD_Y   126.0f
#define PILE_SCALE  0.75f
#define HAND_SCALE  0.75f
#define HAND_Y      100.0f
#define CARD_STEP   33.0f
#define GROUP_GAP   12.0f

static Chinchon s_game;
static Mood s_rival_mood, s_my_mood;
static bool s_started;

static VisCard s_hand[DECK_SIZE]; /* mano del jugador, por card_index */
static VisCard s_incoming;        /* carta que vuela hacia el descarte */
static int s_selected;
static float s_cpu_timer;
static float s_result_time;
static float s_banner_time;       /* cartel de "¡Chinchón!" o "¡Menos diez!" */
static char s_message[96];
static float s_message_flash;

/* Análisis de la mano del jugador, recalculado solo cuando cambia */
static Card s_cached_hand[CH_MAX_HAND];
static int s_cached_count = -1;
static ChAnalysis s_analysis;

static Button s_btn_a, s_btn_b, s_btn_menu;

static void set_message(const char* msg)
{
	snprintf(s_message, sizeof s_message, "%s", msg);
	s_message_flash = 1.0f;
}

static const ChAnalysis* hand_analysis(void)
{
	int n = s_game.hand_count[0];
	if (n != s_cached_count || memcmp(s_cached_hand, s_game.hands[0], sizeof(Card) * n) != 0) {
		memcpy(s_cached_hand, s_game.hands[0], sizeof(Card) * n);
		s_cached_count = n;
		ch_analyze(s_game.hands[0], n, &s_analysis);
	}
	return &s_analysis;
}

/* Grupo al que pertenece la carta i de una mano analizada (-1 = suelta). */
static int group_of(const ChAnalysis* a, int i)
{
	for (int m = 0; m < a->meld_count; m++)
		if (a->melds[m] & (1u << i))
			return m;
	return -1;
}

/* Posiciones x de una fila de cartas, con un hueco entre combinaciones. */
static void row_positions(const ChAnalysis* a, int n, float cx, float step, float gap, float* xs)
{
	float x = 0.0f;
	for (int i = 0; i < n; i++) {
		if (i > 0 && group_of(a, i) != group_of(a, i - 1))
			x += gap;
		xs[i] = x;
		x += step;
	}
	float width = xs[n - 1];
	for (int i = 0; i < n; i++)
		xs[i] += cx - width / 2.0f;
}

static float rival_slot_x(int i, int n)
{
	return 200.0f + (i - (n - 1) / 2.0f) * 22.0f;
}

static void reset_visuals(void)
{
	memset(s_hand, 0, sizeof s_hand);
	memset(&s_incoming, 0, sizeof s_incoming);
	flyers_clear();
	s_selected = 0;
	s_cpu_timer = 0.0f;
	s_result_time = 0.0f;
	s_banner_time = 0.0f;
	s_cached_count = -1;
	char buf[64];
	snprintf(buf, sizeof buf, s_game.current == 0 ? "Empiezas tú: roba una carta" : "Empieza %s", cpu_name(RIVAL));
	set_message(buf);
}

static void ch_scene_enter(void)
{
	if (!s_started) {
		ch_init(&s_game, (uint32_t)time(NULL) ^ 0xC41C40);
		s_started = true;
		reset_visuals();
	}
	button_set(&s_btn_menu, 258, 10, 54, 20, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
}

static bool busy(void)
{
	if (flyers_busy() || vis_moving(&s_incoming))
		return true;
	for (int i = 0; i < DECK_SIZE; i++)
		if (s_hand[i].active && s_hand[i].delay > 0.0f)
			return true;
	return false;
}

/* ---------------------------------------------------------------------------
 * Acciones (las usan tanto el jugador como la CPU) y sus animaciones
 * ------------------------------------------------------------------------- */

static void do_draw(bool from_discard)
{
	int p = s_game.current;
	Card top = s_game.discard_count > 0 ? s_game.discard[s_game.discard_count - 1] : card_make(SUIT_OROS, 1);
	int reshuffles = s_game.reshuffles;
	bool ok = from_discard ? ch_draw_discard(&s_game) : ch_draw_deck(&s_game);
	if (!ok)
		return;

	VisCard v;
	float fx = from_discard ? DISCARD_X : DECK_X, fy = from_discard ? DISCARD_Y : DECK_Y;
	vis_spawn(&v, top, fx, fy, PILE_SCALE, from_discard, 0.0f);
	if (p == 0) {
		/* La carta nueva aparece en la mano por arriba (sync_hand) */
		flyers_add(&v, 200.0f, 290.0f, 0.0f, from_discard, FLY_TOP);
	} else {
		v.tscale = RIVAL_SCALE;
		flyers_add(&v, rival_slot_x(s_game.hand_count[1] - 1, s_game.hand_count[1]), RIVAL_Y, 0.0f, false, FLY_TOP);
	}

	char buf[96], card[32];
	if (s_game.reshuffles != reshuffles)
		set_message("Se acaba el mazo: se barajan los descartes");
	else if (p == 1) {
		if (from_discard) {
			card_describe_article(top, card, sizeof card);
			snprintf(buf, sizeof buf, "%s coge %s", cpu_name(RIVAL), card);
			set_message(buf);
		} else {
			snprintf(buf, sizeof buf, "%s roba del mazo", cpu_name(RIVAL));
			set_message(buf);
		}
	} else {
		set_message("Ahora descarta una carta");
	}
}

static void do_discard(int idx, bool close)
{
	int p = s_game.current;
	float fx = 200.0f, fy = 290.0f, fscale = 0.9f;
	if (p == 1) {
		fx = rival_slot_x(idx, s_game.hand_count[1]);
		fy = RIVAL_Y;
		fscale = RIVAL_SCALE;
	}
	bool ok = close ? ch_close(&s_game, idx) : ch_discard(&s_game, idx);
	if (!ok) {
		audio_play(SFX_ERROR);
		return;
	}
	Card c = s_game.last_discarded;
	vis_spawn(&s_incoming, c, fx, fy, fscale, p == 0, 0.0f);
	vis_target(&s_incoming, DISCARD_X, DISCARD_Y, PILE_SCALE, true);
	s_incoming.tangle = 0.05f;

	char buf[96], card[32];
	card_describe_article(c, card, sizeof card);
	if (close) {
		if (s_game.chinchon) {
			snprintf(buf, sizeof buf, p == 0 ? "¡Chinchón! Ganas la partida" : "¡Chinchón de %s!", cpu_name(RIVAL));
			s_banner_time = 2.0f;
		} else if (s_game.menos_diez) {
			snprintf(buf, sizeof buf, p == 0 ? "¡Cierras con menos diez!" : "%s cierra con menos diez", cpu_name(RIVAL));
			s_banner_time = 2.0f;
		} else {
			snprintf(buf, sizeof buf, p == 0 ? "Cierras la ronda" : "%s cierra la ronda", cpu_name(RIVAL));
		}
		audio_play(p == 0 ? SFX_WIN : SFX_LOSE);
		Expression closer = s_game.chinchon || s_game.menos_diez ? EXPR_SURPRISED : EXPR_HAPPY;
		mood_set(&s_rival_mood, p == 1 ? closer : EXPR_SAD, 4.0f);
		mood_set(&s_my_mood, p == 0 ? closer : EXPR_SAD, 4.0f);
		if (s_game.phase == CH_PHASE_GAME_OVER)
			profile_record_game(GAME_CHINCHON, ch_winner(&s_game) == 0);
	} else {
		if (p == 0)
			snprintf(buf, sizeof buf, "Descartas %s", card);
		else
			snprintf(buf, sizeof buf, "%s descarta %s", cpu_name(RIVAL), card);
	}
	set_message(buf);
	if (s_selected >= s_game.hand_count[0])
		s_selected = s_game.hand_count[0] - 1;
	if (s_selected < 0)
		s_selected = 0;
}

static void sync_hand(void)
{
	const int n = s_game.hand_count[0];
	const ChAnalysis* a = hand_analysis();
	bool present[DECK_SIZE] = { false };
	bool choosing = s_game.phase == CH_PHASE_DISCARD && s_game.current == 0;
	float xs[CH_MAX_HAND];
	float delay = 0.0f;

	if (n > 0)
		row_positions(a, n, BOTTOM_W / 2.0f, CARD_STEP, GROUP_GAP, xs);
	for (int i = 0; i < n; i++) {
		Card c = s_game.hands[0][i];
		int id = card_index(c);
		VisCard* v = &s_hand[id];
		present[id] = true;
		if (!v->active) {
			if (s_game.turns == 0 && s_game.phase == CH_PHASE_DRAW) {
				/* Reparto inicial: las cartas salen del mazo de arriba */
				VisCard tmp;
				vis_spawn(&tmp, c, DECK_X, DECK_Y, PILE_SCALE, false, delay);
				flyers_add(&tmp, 200.0f, 290.0f, delay, false, FLY_TOP);
			}
			vis_spawn(v, c, BOTTOM_W / 2.0f, -60.0f, HAND_SCALE, false, delay + 0.3f);
			delay += 0.12f;
		}
		bool selected = choosing && i == s_selected;
		vis_target(v, xs[i], HAND_Y - (selected ? 12.0f : 0.0f), HAND_SCALE, true);
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
	/* De derecha a izquierda: las de la derecha están encima */
	for (int i = s_game.hand_count[0] - 1; i >= 0; i--) {
		const VisCard* v = &s_hand[card_index(s_game.hands[0][i])];
		float w = CARD_W * HAND_SCALE / 2.0f, h = CARD_H * HAND_SCALE / 2.0f;
		if (in->tx >= v->x - w && in->tx <= v->x + w && in->ty >= v->y - h && in->ty <= v->y + h)
			return i;
	}
	return -1;
}

static void select_suggested_discard(void)
{
	/* Sugerencia: la que descartaría la IA (cerrando si se puede) */
	bool close;
	s_selected = ch_ai_discard(&s_game, 0, &close);
}

static SceneId ch_scene_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;

	bool is_busy = busy();
	const float y = 205, h = 28;
	bool my_turn = s_game.current == 0;

	switch (s_game.phase) {
	case CH_PHASE_DRAW:
		button_set(&s_btn_a, 12, y, 144, h, GLYPH_X " Robar del mazo", C2D_Color32(40, 140, 60, 255), KEY_X);
		button_set(&s_btn_b, 164, y, 144, h, GLYPH_Y " Coger descarte", C2D_Color32(60, 90, 160, 255), KEY_Y);
		s_btn_a.enabled = s_btn_b.enabled = my_turn && !is_busy;
		if (!my_turn) {
			s_cpu_timer += dt;
			if (!is_busy && s_cpu_timer > 0.6f) {
				s_cpu_timer = 0.0f;
				do_draw(ch_ai_take_discard(&s_game, 1));
			}
		} else if (!is_busy) {
			if (button_update(&s_btn_a, in)) {
				do_draw(false);
				select_suggested_discard();
			} else if (button_update(&s_btn_b, in)) {
				do_draw(true);
				select_suggested_discard();
			}
		}
		break;
	case CH_PHASE_DISCARD:
		button_set(&s_btn_a, 12, y, 144, h, GLYPH_A " Descartar", C2D_Color32(40, 140, 60, 255), KEY_A);
		button_set(&s_btn_b, 164, y, 144, h, GLYPH_Y " Cerrar", C2D_Color32(200, 140, 30, 255), KEY_Y);
		s_btn_a.enabled = my_turn && !is_busy;
		s_btn_b.enabled = my_turn && !is_busy && ch_can_close(&s_game, s_selected);
		if (!my_turn) {
			s_cpu_timer += dt;
			if (!is_busy && s_cpu_timer > 0.8f) {
				s_cpu_timer = 0.0f;
				bool close;
				int idx = ch_ai_discard(&s_game, 1, &close);
				do_discard(idx, close);
			}
		} else if (!is_busy) {
			int n = s_game.hand_count[0];
			if (in->repeat & KEY_LEFT) {
				s_selected = (s_selected + n - 1) % n;
				audio_play(SFX_SELECT);
			}
			if (in->repeat & KEY_RIGHT) {
				s_selected = (s_selected + 1) % n;
				audio_play(SFX_SELECT);
			}
			int t = touched_card(in);
			if (t >= 0) {
				/* Primer toque: elegir. Segundo toque sobre la misma carta: descartar. */
				if (t == s_selected) {
					do_discard(t, false);
				} else {
					s_selected = t;
					audio_play(SFX_SELECT);
				}
			} else if (button_update(&s_btn_a, in)) {
				do_discard(s_selected, false);
			} else if (button_update(&s_btn_b, in)) {
				do_discard(s_selected, true);
			}
		}
		break;
	case CH_PHASE_ROUND_OVER:
	case CH_PHASE_GAME_OVER: {
		bool over = s_game.phase == CH_PHASE_GAME_OVER;
		button_set(&s_btn_a, over ? 12 : 60, y, over ? 144 : 200, h,
			over ? GLYPH_A " Otra partida" : GLYPH_A " Siguiente ronda", C2D_Color32(40, 140, 60, 255), KEY_A);
		button_set(&s_btn_b, 164, y, 144, h, GLYPH_B " Menú", C2D_Color32(190, 60, 50, 255), KEY_B);
		s_btn_a.enabled = s_btn_b.enabled = !is_busy && s_result_time > 0.8f;
		if (button_update(&s_btn_a, in)) {
			if (over)
				ch_init(&s_game, (uint32_t)time(NULL));
			else
				ch_next_round(&s_game);
			reset_visuals();
		} else if (over && button_update(&s_btn_b, in)) {
			return SCENE_MENU;
		}
		break;
	}
	}

	if (s_game.phase >= CH_PHASE_ROUND_OVER && !busy())
		s_result_time += dt;

	sync_hand();
	for (int i = 0; i < DECK_SIZE; i++)
		vis_update(&s_hand[i], dt);
	vis_update(&s_incoming, dt);
	if (s_incoming.active && !vis_moving(&s_incoming))
		s_incoming.active = false;
	flyers_update(dt);
	mood_update(&s_rival_mood, dt);
	mood_update(&s_my_mood, dt);
	if (s_banner_time > 0.0f && !busy())
		s_banner_time -= dt;
	if (s_message_flash > 0.0f)
		s_message_flash -= dt * 2.0f;
	return SCENE_CHINCHON;
}

/* ---------------------------------------------------------------------------
 * Dibujo
 * ------------------------------------------------------------------------- */

static void draw_piles(void)
{
	/* Mazo */
	if (s_game.deck.count > 0) {
		int layers = (s_game.deck.count + 3) / 4;
		for (int i = 0; i < layers; i++)
			gfx_card(card_make(SUIT_OROS, 1), DECK_X - i * 0.5f, DECK_Y - i * 0.7f, PILE_SCALE, 0.0f, 0.0f,
				DEPTH_CARDS + i * 0.08f, 0.0f, i == 0);
	} else {
		gfx_rrect(DECK_X - 24, DECK_Y - 36, 48, 72, 4, DEPTH_TABLE, C2D_Color32(0, 0, 0, 50));
	}
	gfx_textf(DECK_X, DECK_Y + 40, DEPTH_UI, 0.42f, COLOR_CREAM, C2D_AlignCenter, "Mazo: %d", s_game.deck.count);

	/* Descartes: mientras una carta vuela hacia el montón, se ve la de debajo */
	int shown = s_game.discard_count - (s_incoming.active ? 1 : 0);
	if (shown <= 0)
		gfx_rrect(DISCARD_X - 24, DISCARD_Y - 36, 48, 72, 4, DEPTH_TABLE, C2D_Color32(0, 0, 0, 50));
	if (shown >= 2)
		gfx_card(s_game.discard[shown - 2], DISCARD_X - 3, DISCARD_Y + 2, PILE_SCALE, 1.0f, -0.08f, DEPTH_CARDS, 0.0f,
			true);
	if (shown >= 1)
		gfx_card(s_game.discard[shown - 1], DISCARD_X, DISCARD_Y, PILE_SCALE, 1.0f, 0.05f, DEPTH_CARDS + 0.2f, 0.0f,
			shown == 1);
	gfx_text("Descarte", DISCARD_X, DISCARD_Y + 40, DEPTH_UI, 0.42f, COLOR_CREAM, C2D_AlignCenter);
	vis_draw(&s_incoming);
}

/* Mano del rival: boca abajo durante la ronda, descubierta y agrupada al final. */
static void draw_rival(void)
{
	int n = s_game.hand_count[1];
	bool reveal = s_game.phase >= CH_PHASE_ROUND_OVER;
	if (!reveal) {
		for (int i = 0; i < n; i++)
			gfx_card(card_make(SUIT_OROS, 1), rival_slot_x(i, n), RIVAL_Y, RIVAL_SCALE, 0.0f,
				(i - (n - 1) / 2.0f) * 0.05f, DEPTH_CARDS, 0.0f, i == 0);
		return;
	}
	const ChAnalysis* a = &s_game.final[1];
	float xs[CH_MAX_HAND];
	row_positions(a, n, 194.0f, 26.0f, 6.0f, xs);
	for (int i = 0; i < n; i++) {
		bool loose = (a->loose >> i) & 1u;
		bool laid = s_game.closer == 0 && ((s_game.laid_off >> i) & 1u);
		gfx_card(s_game.hands[1][i], xs[i], RIVAL_Y + 4, 0.6f, 1.0f, 0.0f, DEPTH_CARDS, loose ? 0.4f : 0.0f, true);
		if (laid)
			gfx_text("liga", xs[i], RIVAL_Y + 34, DEPTH_UI, 0.38f, COLOR_GOLD, C2D_AlignCenter);
	}
}

static void draw_result(void)
{
	if (s_banner_time > 0.0f && (s_game.chinchon || s_game.menos_diez)) {
		float t = 2.0f - s_banner_time;
		float s = ease_out_back(fminf(1.0f, t / 0.3f));
		float alpha = fminf(1.0f, s_banner_time / 0.3f);
		const char* txt = s_game.chinchon ? "¡CHINCHÓN!" : "¡MENOS DIEZ!";
		gfx_text(txt, 200, 120 - 22 * s, DEPTH_BANNER + 1.0f, 1.3f * s, C2D_Color32(255, 210, 80, (u8)(255 * alpha)),
			C2D_AlignCenter);
		return;
	}
	/* Dos capas de cartel para que se lea bien sobre los montones. */
	banner_draw(s_result_time, 200, 150, 320, 100);
	if (!banner_draw(s_result_time, 200, 150, 320, 100))
		return;
	const float d = DEPTH_BANNER;
	bool over = s_game.phase == CH_PHASE_GAME_OVER;
	int closer = s_game.closer;
	if (over) {
		int w = ch_winner(&s_game);
		if (w == 0)
			gfx_text("¡Ganas la partida!", 200, 106, d, 0.7f, COLOR_GOLD, C2D_AlignCenter);
		else
			gfx_textf(200, 106, d, 0.7f, COLOR_RED, C2D_AlignCenter, "Gana %s", cpu_name(RIVAL));
	} else {
		gfx_textf(200, 106, d, 0.7f, COLOR_GOLD, C2D_AlignCenter, closer == 0 ? "Cierras tú" : "Cierra %s",
			cpu_name(RIVAL));
	}
	if (s_game.chinchon) {
		gfx_text("con chinchón: ¡partida ganada!", 200, 134, d, 0.5f, COLOR_CREAM, C2D_AlignCenter);
		return;
	}
	gfx_textf(130, 134, d, 0.5f, COLOR_CREAM, C2D_AlignCenter, "Tú %+d", s_game.round_points[0]);
	gfx_textf(270, 134, d, 0.5f, COLOR_CREAM, C2D_AlignCenter, "%s %+d", cpu_name(RIVAL), s_game.round_points[1]);
	gfx_textf(130, 156, d, 0.6f, COLOR_WHITE, C2D_AlignCenter, "%d", s_game.score[0]);
	gfx_textf(270, 156, d, 0.6f, COLOR_WHITE, C2D_AlignCenter, "%d", s_game.score[1]);
	gfx_textf(200, 182, d, 0.4f, C2D_Color32(200, 230, 200, 255), C2D_AlignCenter, "Pierde quien pase de %d",
		CH_LIMIT);
}

static void ch_scene_draw_top(void)
{
	gfx_background(BG_TABLE_TOP);

	/* Marcador */
	gfx_panel(14, 14, 84, 52, DEPTH_UI, C2D_Color32(10, 50, 25, 210), C2D_Color32(20, 60, 30, 255));
	gfx_textf(56, 17, DEPTH_UI, 0.4f, COLOR_CREAM, C2D_AlignCenter, "Límite %d", CH_LIMIT);
	mii_draw(&profile_get()->avatar, 28, 40, 6, DEPTH_UI, mood_expr(&s_my_mood), false);
	mii_draw(cpu_avatar(RIVAL), 28, 54, 6, DEPTH_UI, mood_expr(&s_rival_mood), false);
	gfx_textf(40, 33, DEPTH_UI, 0.45f, COLOR_WHITE, C2D_AlignLeft, "Tú %d", s_game.score[0]);
	gfx_textf(40, 47, DEPTH_UI, 0.45f, COLOR_WHITE, C2D_AlignLeft, "%s %d", cpu_name(RIVAL), s_game.score[1]);
	gfx_panel(300, 14, 86, 48, DEPTH_UI, C2D_Color32(10, 50, 25, 210),
		s_game.current == 1 && s_game.phase < CH_PHASE_ROUND_OVER ? COLOR_GOLD : C2D_Color32(20, 60, 30, 255));
	mii_draw(cpu_avatar(RIVAL), 318, 38, 13, DEPTH_UI + 0.4f, mood_expr(&s_rival_mood), false);
	gfx_text(cpu_name(RIVAL), 360, 19, DEPTH_UI, 0.48f, COLOR_CREAM, C2D_AlignCenter);
	gfx_textf(360, 39, DEPTH_UI, 0.4f, COLOR_CREAM, C2D_AlignCenter, "Ronda %d", s_game.round);

	draw_piles();
	draw_rival();
	flyers_draw(FLY_TOP);

	if (s_game.phase >= CH_PHASE_ROUND_OVER && !busy())
		draw_result();
}

static void ch_scene_draw_bottom(void)
{
	gfx_background(BG_TABLE_BOTTOM);

	gfx_text(s_message, 14, 12, 0, 0.5f, s_message_flash > 0.0f ? COLOR_GOLD : COLOR_CREAM, C2D_AlignLeft);
	button_draw(&s_btn_menu, false);

	bool my_turn = s_game.current == 0 && s_game.phase < CH_PHASE_ROUND_OVER;
	if (my_turn)
		gfx_text(s_game.phase == CH_PHASE_DRAW ? "¡Tu turno! Roba una carta" : "Elige la carta que descartas",
			BOTTOM_W / 2, 30, 0, 0.5f, COLOR_GOLD, C2D_AlignCenter);
	else if (s_game.phase < CH_PHASE_ROUND_OVER)
		gfx_textf(BOTTOM_W / 2, 30, 0, 0.5f, COLOR_CREAM, C2D_AlignCenter, "Turno de %s...", cpu_name(RIVAL));

	/* Barras doradas bajo cada combinación */
	const ChAnalysis* a = hand_analysis();
	int n = s_game.hand_count[0];
	for (int m = 0; m < a->meld_count; m++) {
		float x0 = 1e9f, x1 = -1e9f;
		for (int i = 0; i < n; i++)
			if ((a->melds[m] >> i) & 1u) {
				const VisCard* v = &s_hand[card_index(s_game.hands[0][i])];
				x0 = fminf(x0, v->x);
				x1 = fmaxf(x1, v->x);
			}
		if (x1 >= x0)
			gfx_rrect(x0 - 22, HAND_Y + 40, x1 - x0 + 44, 5, 2, 0, COLOR_GOLD);
	}

	bool choosing = my_turn && s_game.phase == CH_PHASE_DISCARD && !busy();
	if (choosing && n > 0) {
		const VisCard* v = &s_hand[card_index(s_game.hands[0][s_selected])];
		float pulse = 0.75f + 0.25f * sinf(gfx_time() * 6.0f);
		C2D_ImageTint tint;
		C2D_AlphaImageTint(&tint, pulse);
		gfx_sprite_ex(SPR_CARD_GLOW, v->x, v->y, HAND_SCALE, HAND_SCALE, 0.0f, 0, &tint);
	}
	for (int i = 0; i < n; i++)
		vis_draw(&s_hand[card_index(s_game.hands[0][i])]);
	flyers_draw(FLY_BOTTOM);

	if (n > 0) {
		if (a->loose_count == 0)
			gfx_text("¡Todo combinado!", BOTTOM_W / 2, 152, 0, 0.5f, COLOR_GOLD, C2D_AlignCenter);
		else
			gfx_textf(BOTTOM_W / 2, 152, 0, 0.5f, COLOR_CREAM, C2D_AlignCenter, "Sueltas: %d (%d puntos)",
				a->loose_count, a->loose_value);
	}
	if (choosing && ch_can_close(&s_game, s_selected))
		gfx_text("¡Puedes cerrar descartando esta carta! (" GLYPH_Y ")", BOTTOM_W / 2, 174, 0, 0.42f, COLOR_GOLD,
			C2D_AlignCenter);
	else
		gfx_textf(BOTTOM_W / 2, 174, 0, 0.42f, C2D_Color32(200, 230, 200, 255), C2D_AlignCenter,
			"Cierras con todo ligado o una suelta de %d o menos", CH_CLOSE_MAX);

	button_draw(&s_btn_a, s_btn_a.enabled);
	if (s_game.phase != CH_PHASE_ROUND_OVER)
		button_draw(&s_btn_b, s_btn_b.enabled && s_game.phase == CH_PHASE_DISCARD);
}

/* Salir a medias desde la pausa: la próxima vez se empieza de cero */
static void scene_leave(void)
{
	s_started = false;
}

const Scene scene_chinchon = { ch_scene_enter, ch_scene_update, ch_scene_draw_top, ch_scene_draw_bottom, scene_leave };
