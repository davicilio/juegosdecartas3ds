/* Mesas del guiñote, el tute y el tute cabrón (cuatro jugadores). Por parejas,
 * tu compañero se sienta arriba y los rivales a los lados. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/guinote.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/flyers.h"
#include "ui/gfx.h"
#include "ui/mii.h"
#include "ui/widgets.h"

/* Asientos: 0 tú (abajo), 1 derecha, 2 arriba (compañero), 3 izquierda */
static const CpuId SEAT_CPU[GN_VARIANTS][GN_PLAYERS] = {
	[GN_GUINOTE] = { CPU_COUNT, CPU_LUCIA, CPU_PEPE, CPU_CARMEN },
	[GN_TUTE] = { CPU_COUNT, CPU_PACO, CPU_LOLA, CPU_MANOLO },
	[GN_TUTE_CABRON] = { CPU_COUNT, CPU_LUCIA, CPU_PEPE, CPU_CARMEN },
	[GN_TUTE_SUBASTADO] = { CPU_COUNT, CPU_MANOLO, CPU_CARMEN, CPU_RAMON },
};
static const SceneId SCENE_OF[GN_VARIANTS] = { SCENE_GUINOTE, SCENE_TUTE, SCENE_TUTE_CABRON, SCENE_TUTE_SUBASTADO };
static const GameId GAME_OF[GN_VARIANTS] = { GAME_GUINOTE, GAME_TUTE, GAME_TUTE_CABRON, GAME_TUTE_SUBASTADO };
static const float SEAT_X[GN_PLAYERS] = { 200, 360, 200, 40 };
static const float SEAT_Y[GN_PLAYERS] = { 290, 78, 30, 78 };
static const float TRICK_X[GN_PLAYERS] = { 200, 244, 200, 156 };
static const float TRICK_Y[GN_PLAYERS] = { 152, 116, 82, 116 };

#define TRICK_SCALE 0.6f
#define HAND_SCALE  0.78f
#define HAND_Y      108.0f
#define DECK_X      300.0f
#define DECK_Y      202.0f

static Guinote s_games[GN_VARIANTS];
static bool s_started[GN_VARIANTS];
static GnVariant s_variant;
#define G (&s_games[s_variant])
#define PAIRS (G->variant != GN_TUTE_CABRON)
#define SUB   (G->variant == GN_TUTE_SUBASTADO)
static int s_bid_amount = GN_MIN_BID;
static Button s_btn_suit[NUM_SUITS];
static Mood s_mood[GN_PLAYERS];
static VisCard s_hand[DECK_SIZE]; /* tus cartas, por card_index */
static VisCard s_trick[GN_PLAYERS];
static int s_selected;
static float s_cpu_timer, s_trick_timer, s_result_time;
static GnPhase s_prev_phase;
static char s_message[96];
static float s_message_flash;
static char s_bubble[GN_PLAYERS][24];
static float s_bubble_time[GN_PLAYERS];
static Button s_btn_play, s_btn_sing, s_btn_swap, s_btn_menu;

/* Puntos de la partida (guiñote), contando los de la mano en curso */
static int total_points(int team)
{
	bool counted = G->phase == GN_PHASE_HAND_OVER || G->phase == GN_PHASE_GAME_OVER;
	return G->score[team] + (counted ? 0 : G->points[team]);
}

static const char* seat_name(int p)
{
	return p == 0 ? profile_get()->avatar.name : cpu_name(SEAT_CPU[s_variant][p]);
}

static const Avatar* seat_avatar(int p)
{
	return p == 0 ? &profile_get()->avatar : cpu_avatar(SEAT_CPU[s_variant][p]);
}

static void set_message(const char* msg)
{
	snprintf(s_message, sizeof s_message, "%s", msg);
	s_message_flash = 1.0f;
}

static void bubble(int p, const char* text)
{
	snprintf(s_bubble[p], sizeof s_bubble[p], "%s", text);
	s_bubble_time[p] = 2.0f;
}

static void reset_visuals(void)
{
	memset(s_hand, 0, sizeof s_hand);
	memset(s_trick, 0, sizeof s_trick);
	memset(s_bubble_time, 0, sizeof s_bubble_time);
	flyers_clear();
	s_selected = 0;
	s_cpu_timer = s_trick_timer = s_result_time = 0.0f;
	s_prev_phase = G->phase;
	char buf[96];
	s_bid_amount = GN_MIN_BID;
	if (G->phase == GN_PHASE_BIDDING)
		snprintf(buf, sizeof buf, "Subasta: empieza a pujar %s", G->current == 0 ? "tú" : seat_name(G->current));
	else if (G->current == 0)
		snprintf(buf, sizeof buf, "Triunfo: %s. Sales tú", suit_name((Suit)G->trump));
	else
		snprintf(buf, sizeof buf, "Triunfo: %s. Sale %s", suit_name((Suit)G->trump), seat_name(G->current));
	set_message(buf);
}

static void enter_variant(GnVariant v)
{
	bool changed = s_variant != v;
	s_variant = v;
	if (!s_started[v]) {
		gn_init(G, v, (uint32_t)time(NULL) ^ (0x6017E + v * 7919));
		s_started[v] = true;
		reset_visuals();
	} else if (changed) {
		reset_visuals();
	}
	button_set(&s_btn_menu, 258, 6, 54, 20, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
	static const char* const names[NUM_SUITS] = { "Oros", "Copas", "Espadas", "Bastos" };
	for (int i = 0; i < NUM_SUITS; i++)
		button_set(&s_btn_suit[i], 20 + (i % 2) * 144, 60 + (i / 2) * 60, 136, 50, names[i],
			C2D_Color32(60, 90, 160, 255), 0);
}

static void guinote_enter(void)
{
	enter_variant(GN_GUINOTE);
}

static void tute_enter(void)
{
	enter_variant(GN_TUTE);
}

static void cabron_enter(void)
{
	enter_variant(GN_TUTE_CABRON);
}

static void subastado_enter(void)
{
	enter_variant(GN_TUTE_SUBASTADO);
}

/* Tute subastado: alguien puja o pasa */
static void after_bid(void)
{
	int p = G->last_bid_player;
	char buf[96];
	if (G->last_bid > 0) {
		char num[12];
		snprintf(num, sizeof num, "%d", G->last_bid);
		bubble(p, num);
		if (p == 0)
			snprintf(buf, sizeof buf, "Pujas %d", G->last_bid);
		else
			snprintf(buf, sizeof buf, "%s puja %d", seat_name(p), G->last_bid);
	} else {
		bubble(p, "Paso");
		if (p == 0)
			snprintf(buf, sizeof buf, "Pasas");
		else
			snprintf(buf, sizeof buf, "%s pasa", seat_name(p));
	}
	set_message(buf);
	audio_play(SFX_SELECT);
	if (G->phase == GN_PHASE_CHOOSE_TRUMP) {
		if (G->bidder == 0)
			snprintf(buf, sizeof buf, "Ganas la subasta con %d: elige el triunfo", G->bid);
		else
			snprintf(buf, sizeof buf, "%s gana la subasta con %d", seat_name(G->bidder), G->bid);
		set_message(buf);
	}
}

static void after_trump(void)
{
	char buf[96];
	snprintf(buf, sizeof buf, "Triunfo: %s. %s", suit_name((Suit)G->trump),
		G->current == 0 ? "Sales tú" : "Sale quien pujó");
	set_message(buf);
	bubble(G->bidder, suit_name((Suit)G->trump));
	audio_play(SFX_FLIP);
}

static bool busy(void)
{
	if (flyers_busy())
		return true;
	for (int p = 0; p < GN_PLAYERS; p++)
		if (vis_moving(&s_trick[p]))
			return true;
	for (int i = 0; i < DECK_SIZE; i++)
		if (s_hand[i].active && s_hand[i].delay > 0.0f)
			return true;
	return false;
}

static void sync_hand(void)
{
	const int n = G->hand_count[0];
	bool present[DECK_SIZE] = { false };
	bool my_turn = G->phase == GN_PHASE_PLAYING && G->current == 0;
	float delay = 0.0f;
	for (int i = 0; i < n; i++) {
		Card c = G->hands[0][i];
		int id = card_index(c);
		VisCard* v = &s_hand[id];
		present[id] = true;
		if (!v->active) {
			vis_spawn(v, c, BOTTOM_W / 2.0f, -60.0f, HAND_SCALE, false, delay);
			delay += 0.12f;
		}
		bool selected = my_turn && i == s_selected;
		float step = n > 1 ? fminf(50.0f, 268.0f / (n - 1)) : 0.0f;
		vis_target(v, BOTTOM_W / 2.0f + (i - (n - 1) / 2.0f) * step, HAND_Y - (selected ? 12.0f : 0.0f),
			HAND_SCALE, true);
		/* En el arrastre se oscurecen las cartas que no se pueden jugar */
		v->dim = my_turn && !gn_can_play(G, 0, i) ? 0.45f : 0.0f;
	}
	for (int id = 0; id < DECK_SIZE; id++)
		if (s_hand[id].active && !present[id])
			s_hand[id].active = false;
}

static void after_play(int p)
{
	VisCard* v = &s_trick[p];
	float fx = p == 0 ? 200.0f : SEAT_X[p], fy = p == 0 ? 260.0f : SEAT_Y[p];
	vis_spawn(v, G->trick[p], fx, fy, 0.5f, p == 0, 0.0f);
	vis_target(v, TRICK_X[p], TRICK_Y[p], TRICK_SCALE, true);
	v->tangle = (p - 1.5f) * 0.06f;
	v->depth = DEPTH_CARDS + G->played_in_trick * 0.1f;
	if (G->phase == GN_PHASE_TRICK_DONE) {
		char buf[96];
		int w = G->trick_winner;
		if (w == 0)
			snprintf(buf, sizeof buf, "Te llevas la baza (+%d)", G->trick_points);
		else
			snprintf(buf, sizeof buf, "%s se lleva la baza (+%d)", seat_name(w), G->trick_points);
		set_message(buf);
		s_trick_timer = 0.0f;
	}
}

static void announce_cante(void)
{
	int p = G->last_cante_player;
	char buf[96];
	if (G->last_cante_suit < 0) {
		bubble(p, "¡Tute!");
		snprintf(buf, sizeof buf, "%s canta el tute y gana la mano", p == 0 ? "Tú" : seat_name(p));
		set_message(buf);
		audio_play(SFX_WIN);
		return;
	}
	bubble(p, G->last_cante_points == 40 ? "¡Las cuarenta!" : "¡Las veinte!");
	snprintf(buf, sizeof buf, "%s canta %s en %s", p == 0 ? "Tú" : seat_name(p),
		G->last_cante_points == 40 ? "las cuarenta" : "las veinte", suit_name((Suit)G->last_cante_suit));
	set_message(buf);
	audio_play(SFX_WIN);
	mood_set(&s_mood[p], EXPR_HAPPY, 1.8f);
	for (int q = 0; q < GN_PLAYERS; q++)
		if (gn_side(G, q) != gn_side(G, p))
			mood_set(&s_mood[q], EXPR_SURPRISED, 1.4f);
}

static void collect(void)
{
	int w = G->trick_winner;
	if (G->trick_points >= 15)
		for (int p = 0; p < GN_PLAYERS; p++)
			mood_set(&s_mood[p], gn_side(G, p) == gn_side(G, w) ? EXPR_HAPPY : EXPR_SAD, 1.4f);
	gn_collect(G);
	for (int p = 0; p < GN_PLAYERS; p++) {
		flyers_add(&s_trick[p], SEAT_X[w], w == 0 ? 260.0f : SEAT_Y[w], p * 0.06f, false, FLY_TOP);
		s_trick[p].active = false;
	}
	audio_play(SFX_CARD);
	if (G->variant == GN_GUINOTE && G->phase == GN_PHASE_PLAYING && gn_arrastre(G) && G->hand_count[0] == 6)
		set_message("¡Arrastre! Asiste y mata si puedes");
	s_selected = 0;
}

static void human_play(int idx)
{
	if (!gn_can_play(G, 0, idx)) {
		audio_play(SFX_ERROR);
		set_message("Hay que asistir, montar o fallar");
		return;
	}
	gn_play(G, idx);
	after_play(0);
	if (s_selected >= G->hand_count[0])
		s_selected = G->hand_count[0] - 1;
	if (s_selected < 0)
		s_selected = 0;
}

static int touched_card(const Input* in)
{
	if (!in->touch_down)
		return -1;
	for (int i = G->hand_count[0] - 1; i >= 0; i--) {
		const VisCard* v = &s_hand[card_index(G->hands[0][i])];
		float w = CARD_W * HAND_SCALE / 2.0f, h = CARD_H * HAND_SCALE / 2.0f;
		if (in->tx >= v->x - w && in->tx <= v->x + w && in->ty >= v->y - h && in->ty <= v->y + h)
			return i;
	}
	return -1;
}

static void cpu_turn(int p)
{
	if (gn_sing_tute(G, p)) {
		announce_cante();
		return;
	}
	if (gn_swap_index(G, p) >= 0) {
		gn_swap(G, p);
		audio_play(SFX_FLIP);
		bubble(p, "Cambio el 7");
		return;
	}
	if (gn_cante(G, p)) {
		announce_cante();
		return;
	}
	int i = gn_ai_choose(G, p);
	if (i >= 0) {
		gn_play(G, i);
		after_play(p);
	}
}

static SceneId gn_scene_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;
	bool is_busy = busy();
	const float y = 205, h = 28;

	switch (G->phase) {
	case GN_PHASE_PLAYING:
		button_set(&s_btn_play, 8, y, 96, h, GLYPH_A " Jugar", C2D_Color32(40, 140, 60, 255), KEY_A);
		button_set(&s_btn_sing, 112, y, 96, h, GLYPH_X " Cantar", C2D_Color32(190, 60, 120, 255), KEY_X);
		button_set(&s_btn_swap, 216, y, 96, h, G->variant == GN_GUINOTE ? GLYPH_Y " Cambiar 7" : GLYPH_Y " ¡Tute!",
			C2D_Color32(200, 140, 30, 255), KEY_Y);
		if (G->current == 0) {
			s_btn_play.enabled = !is_busy;
			s_btn_sing.enabled = !is_busy && gn_cante_suit(G, 0) >= 0;
			s_btn_swap.enabled = !is_busy && (gn_swap_index(G, 0) >= 0 || gn_can_sing_tute(G, 0));
			if (is_busy)
				break;
			int n = G->hand_count[0];
			if (n == 0)
				break;
			/* El cursor empieza en una carta que se pueda jugar */
			if (!gn_can_play(G, 0, s_selected))
				for (int i = 0; i < n; i++)
					if (gn_can_play(G, 0, i)) {
						s_selected = i;
						break;
					}
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
				s_selected = t;
				human_play(t);
			} else if (button_update(&s_btn_play, in)) {
				human_play(s_selected);
			} else if (button_update(&s_btn_sing, in)) {
				if (gn_cante(G, 0))
					announce_cante();
			} else if (button_update(&s_btn_swap, in)) {
				if (gn_sing_tute(G, 0)) {
					announce_cante();
				} else if (gn_swap(G, 0)) {
					audio_play(SFX_FLIP);
					set_message("Has cambiado el 7 por la muestra");
				}
			}
		} else {
			s_btn_play.enabled = s_btn_sing.enabled = s_btn_swap.enabled = false;
			s_cpu_timer += dt;
			if (!is_busy && s_cpu_timer > 0.75f) {
				s_cpu_timer = 0.0f;
				cpu_turn(G->current);
			}
		}
		break;
	case GN_PHASE_BIDDING: {
		int min = gn_min_bid(G);
		static char label[24];
		if (s_bid_amount < min)
			s_bid_amount = min;
		if (s_bid_amount > GN_MAX_BID)
			s_bid_amount = GN_MAX_BID;
		snprintf(label, sizeof label, GLYPH_A " Pujo %d", s_bid_amount);
		button_set(&s_btn_play, 12, y, 144, h, label, C2D_Color32(40, 140, 60, 255), KEY_A);
		button_set(&s_btn_swap, 164, y, 144, h, GLYPH_B " Paso", C2D_Color32(190, 60, 50, 255), KEY_B);
		bool mine = G->current == 0;
		s_btn_play.enabled = mine && min <= GN_MAX_BID;
		s_btn_swap.enabled = mine && !gn_must_bid(G);
		s_btn_sing.enabled = false;
		if (mine) {
			if (in->repeat & (KEY_R | KEY_UP | KEY_RIGHT))
				s_bid_amount += GN_BID_STEP;
			if (in->repeat & (KEY_L | KEY_DOWN | KEY_LEFT))
				s_bid_amount -= GN_BID_STEP;
			if (s_bid_amount < min)
				s_bid_amount = min;
			if (s_bid_amount > GN_MAX_BID)
				s_bid_amount = GN_MAX_BID;
			if (button_update(&s_btn_play, in)) {
				if (gn_bid(G, s_bid_amount))
					after_bid();
			} else if (button_update(&s_btn_swap, in)) {
				if (gn_bid(G, 0))
					after_bid();
			}
		} else {
			s_cpu_timer += dt;
			if (s_cpu_timer > 0.9f) {
				s_cpu_timer = 0.0f;
				gn_bid(G, gn_ai_bid(G, G->current));
				after_bid();
			}
		}
		break;
	}
	case GN_PHASE_CHOOSE_TRUMP:
		s_btn_play.enabled = s_btn_sing.enabled = s_btn_swap.enabled = false;
		if (G->current == 0) {
			for (int i = 0; i < NUM_SUITS; i++)
				if (button_update(&s_btn_suit[i], in) && gn_choose_trump(G, i)) {
					after_trump();
					break;
				}
		} else {
			s_cpu_timer += dt;
			if (s_cpu_timer > 0.9f) {
				s_cpu_timer = 0.0f;
				gn_choose_trump(G, gn_ai_trump(G, G->current));
				after_trump();
			}
		}
		break;
	case GN_PHASE_TRICK_DONE:
		s_btn_play.enabled = s_btn_sing.enabled = s_btn_swap.enabled = false;
		if (!is_busy) {
			s_trick_timer += dt;
			if (s_trick_timer > 1.1f)
				collect();
		}
		break;
	case GN_PHASE_HAND_OVER:
	case GN_PHASE_GAME_OVER: {
		bool game_over = G->phase == GN_PHASE_GAME_OVER;
		button_set(&s_btn_play, 12, y, 144, h, game_over ? GLYPH_A " Otra partida" : GLYPH_A " Siguiente mano",
			C2D_Color32(40, 140, 60, 255), KEY_A);
		button_set(&s_btn_swap, 164, y, 144, h, GLYPH_B " Menú", C2D_Color32(190, 60, 50, 255), KEY_B);
		s_btn_play.enabled = s_btn_swap.enabled = !is_busy && s_result_time > 0.6f;
		s_btn_sing.enabled = false;
		if (button_update(&s_btn_play, in)) {
			if (game_over)
				gn_new_game(G);
			else
				gn_new_hand(G);
			reset_visuals();
		} else if (button_update(&s_btn_swap, in)) {
			return SCENE_MENU;
		}
		break;
	}
	}

	if (G->phase != s_prev_phase) {
		s_prev_phase = G->phase;
		s_cpu_timer = 0.0f;
		s_result_time = 0.0f;
	}
	if ((G->phase == GN_PHASE_HAND_OVER || G->phase == GN_PHASE_GAME_OVER) && !is_busy) {
		if (s_result_time == 0.0f) {
			int hand_winner = G->hand_winner;
			if (G->phase == GN_PHASE_GAME_OVER) {
				int w = gn_game_winner(G);
				audio_play(w == 0 ? SFX_WIN : SFX_LOSE);
				profile_record_game(GAME_OF[s_variant], w == 0);
				hand_winner = w;
			}
			for (int p = 0; p < GN_PLAYERS; p++)
				mood_set(&s_mood[p], gn_side(G, p) == hand_winner ? EXPR_HAPPY : EXPR_SAD, 4.0f);
		}
		s_result_time += dt;
	}

	sync_hand();
	for (int i = 0; i < DECK_SIZE; i++)
		vis_update(&s_hand[i], dt);
	for (int p = 0; p < GN_PLAYERS; p++) {
		vis_update(&s_trick[p], dt);
		mood_update(&s_mood[p], dt);
		if (s_bubble_time[p] > 0.0f)
			s_bubble_time[p] -= dt;
	}
	flyers_update(dt);
	if (s_message_flash > 0.0f)
		s_message_flash -= dt * 2.0f;
	return SCENE_OF[s_variant];
}

/* ---------------------------------------------------------------------------
 * Dibujo
 * ------------------------------------------------------------------------- */

static void draw_seat(int p)
{
	float x = SEAT_X[p], y = SEAT_Y[p];
	int n = G->hand_count[p];
	/* Cartas boca abajo */
	for (int i = 0; i < n; i++) {
		float off = (i - (n - 1) / 2.0f);
		if (p == 2)
			gfx_card_back(x + 40 + off * 12.0f, y, 0.42f, 0.42f, off * 0.06f, DEPTH_CARDS + i * 0.02f, NULL);
		else
			gfx_card_back(x + off * 4.0f, y + 46 + off * 3.0f, 0.42f, 0.42f, (p == 1 ? -1.0f : 1.0f) * (0.3f + off * 0.05f),
				DEPTH_CARDS + i * 0.02f, NULL);
	}
	float ax = p == 2 ? x - 50 : x;
	mii_draw(seat_avatar(p), ax, y, 17, DEPTH_UI + 0.5f, mood_expr(&s_mood[p]), false);
	bool turn = G->current == p && (G->phase == GN_PHASE_PLAYING || G->phase == GN_PHASE_BIDDING ||
	                                 G->phase == GN_PHASE_CHOOSE_TRUMP);
	u32 border = turn ? COLOR_GOLD : gn_side(G, p) == gn_side(G, 0) ? C2D_Color32(60, 120, 200, 255) : C2D_Color32(190, 60, 50, 255);
	float nx = p == 2 ? x - 50 : x, ny = y + 20;
	gfx_panel(nx - 36, ny, 72, 18, DEPTH_UI, C2D_Color32(10, 50, 25, 220), border);
	gfx_text(seat_name(p), nx, ny + 2, DEPTH_UI, 0.42f, COLOR_CREAM, C2D_AlignCenter);
}

static void draw_bubble(int p)
{
	if (s_bubble_time[p] <= 0.0f)
		return;
	float bx = p == 0 ? 200 : p == 2 ? 300 : SEAT_X[p], by = p == 0 ? 196 : p == 2 ? 30 : SEAT_Y[p] - 30;
	if (p == 1)
		bx -= 40;
	if (p == 3)
		bx += 40;
	float pop = ease_out_back(fminf(1.0f, (2.0f - s_bubble_time[p]) / 0.2f));
	float w = gfx_text_width(s_bubble[p], 0.5f) + 16;
	gfx_rrect(bx - w / 2 * pop, by - 11 * pop, w * pop, 22 * pop, 8, DEPTH_BANNER, C2D_Color32(255, 250, 235, 240));
	if (pop > 0.8f)
		gfx_text(s_bubble[p], bx, by - 8, DEPTH_BANNER, 0.5f, C2D_Color32(120, 20, 20, 255), C2D_AlignCenter);
}

static void draw_result(void)
{
	if (!banner_draw(s_result_time, 200, 118, 340, 100))
		return;
	bool over = G->phase == GN_PHASE_GAME_OVER;
	int w = over ? gn_game_winner(G) : G->hand_winner;
	char title[64];
	if (G->variant == GN_GUINOTE && !over)
		snprintf(title, sizeof title, "Nadie llega a 101: ¡de vueltas!");
	else if (SUB && !over)
		snprintf(title, sizeof title, G->bid_made ? "¡%s la subasta!" : "%s la subasta",
			G->bid_made ? (gn_side(G, G->bidder) == 0 ? "Cumplís" : "Cumplen")
			            : (gn_side(G, G->bidder) == 0 ? "No llegáis a" : "No llegan a"));
	else if (PAIRS)
		snprintf(title, sizeof title, over ? (w == 0 ? "¡Ganáis la partida!" : "Ganan los rivales")
		                                   : (w == 0 ? "¡Ganáis la mano!" : "Los rivales ganan la mano"));
	else if (w == 0)
		snprintf(title, sizeof title, over ? "¡Ganas la partida!" : "¡Ganas la mano!");
	else
		snprintf(title, sizeof title, over ? "Gana la partida %s" : "Gana la mano %s", seat_name(w));
	gfx_text(title, 200, 74, DEPTH_BANNER, 0.62f, w == 0 ? COLOR_GOLD : COLOR_RED, C2D_AlignCenter);

	if (G->tute) {
		gfx_text("¡Con el tute!", 200, 100, DEPTH_BANNER, 0.55f, COLOR_CREAM, C2D_AlignCenter);
	} else if (SUB) {
		int bs = gn_side(G, G->bidder);
		gfx_textf(200, 100, DEPTH_BANNER, 0.47f, COLOR_CREAM, C2D_AlignCenter, "%s %d y %s %d",
			bs == 0 ? "Pujasteis" : "Pujaron", G->bid, bs == 0 ? "hicisteis" : "hicieron", G->points[bs]);
	} else if (PAIRS) {
		gfx_textf(200, 100, DEPTH_BANNER, 0.5f, COLOR_CREAM, C2D_AlignCenter, "Esta mano: nosotros %d - ellos %d",
			G->points[0], G->points[1]);
	} else {
		gfx_textf(200, 100, DEPTH_BANNER, 0.45f, COLOR_CREAM, C2D_AlignCenter, "Tú %d · %s %d · %s %d · %s %d",
			G->points[0], seat_name(1), G->points[1], seat_name(2), G->points[2], seat_name(3), G->points[3]);
	}
	if (G->variant == GN_GUINOTE)
		gfx_textf(200, 124, DEPTH_BANNER, 0.55f, COLOR_GOLD, C2D_AlignCenter, "Total: nosotros %d - ellos %d",
			G->score[0], G->score[1]);
	else if (SUB)
		gfx_textf(200, 124, DEPTH_BANNER, 0.55f, COLOR_GOLD, C2D_AlignCenter, "Total: nosotros %d - ellos %d (a %d)",
			G->score[0], G->score[1], GN_SUB_POINTS);
	else if (PAIRS)
		gfx_textf(200, 124, DEPTH_BANNER, 0.55f, COLOR_GOLD, C2D_AlignCenter, "Manos: nosotros %d - ellos %d (a %d)",
			G->score[0], G->score[1], GN_TUTE_HANDS);
	else
		gfx_textf(200, 124, DEPTH_BANNER, 0.5f, COLOR_GOLD, C2D_AlignCenter, "Manos: tú %d · %d · %d · %d (a %d)",
			G->score[0], G->score[1], G->score[2], G->score[3], GN_TUTE_HANDS);
}

static void gn_scene_draw_top(void)
{
	gfx_background(BG_TABLE_TOP);

	/* Mazo con la muestra atravesada debajo */
	if (G->deck.count > 0) {
		gfx_card(G->trump_card, DECK_X + 24, DECK_Y, 0.55f, 1.0f, (float)M_PI / 2.0f, DEPTH_CARDS, 0.0f, true);
		int layers = (G->deck.count - 1 + 3) / 4;
		for (int i = 0; i < layers; i++)
			gfx_card_back(DECK_X - i * 0.5f, DECK_Y - i * 0.7f, 0.55f, 0.55f, 0.0f, DEPTH_CARDS + 0.3f + i * 0.08f, NULL);
		gfx_textf(DECK_X + 10, DECK_Y - 44, DEPTH_UI, 0.4f, COLOR_CREAM, C2D_AlignCenter, "Mazo: %d",
			G->deck.count);
	} else if (SUB) {
		gfx_text("Subasta", DECK_X + 10, DECK_Y - 44, DEPTH_UI, 0.42f, COLOR_CREAM, C2D_AlignCenter);
		if (G->bidder >= 0) {
			gfx_textf(DECK_X + 10, DECK_Y - 26, DEPTH_UI, 0.6f, COLOR_GOLD, C2D_AlignCenter, "%d", G->bid);
			gfx_text(G->bidder == 0 ? "Tú" : seat_name(G->bidder), DECK_X + 10, DECK_Y - 6, DEPTH_UI, 0.42f,
				COLOR_CREAM, C2D_AlignCenter);
		}
		if (G->trump >= 0)
			gfx_sprite(SPR_SUIT_FIRST + G->trump, DECK_X + 10, DECK_Y + 22, 0.8f, DEPTH_UI);
	} else if (G->variant == GN_GUINOTE) {
		gfx_text("Arrastre", DECK_X + 10, DECK_Y - 8, DEPTH_UI, 0.55f, COLOR_GOLD, C2D_AlignCenter);
	} else {
		/* Tute: la pinta es la última carta que se repartió */
		gfx_text("Pinta", DECK_X + 10, DECK_Y - 44, DEPTH_UI, 0.42f, COLOR_CREAM, C2D_AlignCenter);
		gfx_card(G->trump_card, DECK_X + 10, DECK_Y, 0.5f, 1.0f, 0.0f, DEPTH_CARDS, 0.0f, true);
	}

	/* Marcador */
	gfx_panel(8, 168, 112, 64, DEPTH_UI, C2D_Color32(10, 50, 25, 210), C2D_Color32(20, 60, 30, 255));
	if (G->trump >= 0) {
		gfx_sprite(SPR_SUIT_FIRST + G->trump, 24, 182, 0.55f, DEPTH_UI);
		gfx_text(suit_name((Suit)G->trump), 38, 174, DEPTH_UI, 0.42f, COLOR_GOLD, C2D_AlignLeft);
	} else {
		gfx_text("Sin triunfo", 64, 174, DEPTH_UI, 0.42f, COLOR_GOLD, C2D_AlignCenter);
	}
	if (G->variant == GN_GUINOTE) {
		gfx_textf(64, 192, DEPTH_UI, 0.42f, C2D_Color32(140, 200, 255, 255), C2D_AlignCenter, "Nosotros %d",
			total_points(0));
		gfx_textf(64, 208, DEPTH_UI, 0.42f, C2D_Color32(255, 150, 140, 255), C2D_AlignCenter, "Ellos %d",
			total_points(1));
	} else if (PAIRS) {
		gfx_textf(64, 192, DEPTH_UI, 0.42f, C2D_Color32(140, 200, 255, 255), C2D_AlignCenter, "Nosotros %d",
			G->points[0]);
		gfx_textf(64, 208, DEPTH_UI, 0.42f, C2D_Color32(255, 150, 140, 255), C2D_AlignCenter, "Ellos %d",
			G->points[1]);
	} else {
		gfx_textf(64, 192, DEPTH_UI, 0.42f, C2D_Color32(140, 200, 255, 255), C2D_AlignCenter, "Tú %d", G->points[0]);
		gfx_textf(64, 208, DEPTH_UI, 0.4f, C2D_Color32(255, 150, 140, 255), C2D_AlignCenter, "%d · %d · %d",
			G->points[1], G->points[2], G->points[3]);
	}

	for (int p = 1; p < GN_PLAYERS; p++)
		draw_seat(p);
	mii_draw(seat_avatar(0), 200, 212, 15, DEPTH_UI + 0.5f, mood_expr(&s_mood[0]), false);

	/* Baza: la primera carta debajo */
	for (int k = 0; k < GN_PLAYERS; k++)
		vis_draw(&s_trick[(G->leader + k) % GN_PLAYERS]);
	flyers_draw(FLY_TOP);
	for (int p = 0; p < GN_PLAYERS; p++)
		draw_bubble(p);

	if ((G->phase == GN_PHASE_HAND_OVER || G->phase == GN_PHASE_GAME_OVER) && !busy())
		draw_result();
}

static void gn_scene_draw_bottom(void)
{
	gfx_background(BG_TABLE_BOTTOM);
	gfx_text(s_message, 10, 10, 0, 0.45f, s_message_flash > 0.0f ? COLOR_GOLD : COLOR_CREAM, C2D_AlignLeft);
	button_draw(&s_btn_menu, false);

	bool my_turn = G->phase == GN_PHASE_PLAYING && G->current == 0;
	if (G->phase == GN_PHASE_CHOOSE_TRUMP && G->current == 0) {
		gfx_text("Elige el triunfo", BOTTOM_W / 2, 30, 0, 0.6f, COLOR_GOLD, C2D_AlignCenter);
		for (int i = 0; i < NUM_SUITS; i++) {
			button_draw(&s_btn_suit[i], true);
			gfx_sprite_ex(SPR_SUIT_FIRST + i, s_btn_suit[i].x + 22, s_btn_suit[i].y + 25, 0.8f, 0.8f, 0.0f, 0, NULL);
		}
		return;
	}
	if (my_turn)
		gfx_text("¡Tu turno!", BOTTOM_W / 2, 32, 0, 0.55f, COLOR_GOLD, C2D_AlignCenter);
	else if (G->phase == GN_PHASE_BIDDING && G->current == 0)
		gfx_text(gn_must_bid(G) ? "Han pasado todos: tienes que pujar" : "¿Cuánto pujas? (L/R para cambiar)",
			BOTTOM_W / 2, 32, 0, 0.5f, COLOR_GOLD, C2D_AlignCenter);
	if (my_turn && G->hand_count[0] > 0 && !busy()) {
		const VisCard* v = &s_hand[card_index(G->hands[0][s_selected])];
		float pulse = 0.75f + 0.25f * sinf(gfx_time() * 6.0f);
		C2D_ImageTint tint;
		C2D_AlphaImageTint(&tint, pulse);
		gfx_sprite_ex(SPR_CARD_GLOW, v->x, v->y, HAND_SCALE, HAND_SCALE, 0.0f, 0, &tint);
	}
	for (int i = 0; i < G->hand_count[0]; i++)
		vis_draw(&s_hand[card_index(G->hands[0][i])]);
	flyers_draw(FLY_BOTTOM);

	if (G->variant == GN_GUINOTE)
		gfx_textf(BOTTOM_W / 2, 160, 0, 0.45f, COLOR_CREAM, C2D_AlignCenter, "Nosotros %d - Ellos %d   (a %d)",
			total_points(0), total_points(1), GN_GAME_POINTS);
	else if (SUB)
		gfx_textf(BOTTOM_W / 2, 160, 0, 0.45f, COLOR_CREAM, C2D_AlignCenter, "Partida: nosotros %d - ellos %d   (a %d)",
			G->score[0], G->score[1], GN_SUB_POINTS);
	else
		gfx_textf(BOTTOM_W / 2, 160, 0, 0.45f, COLOR_CREAM, C2D_AlignCenter, "Manos ganadas: %s %d - %s %d   (a %d)",
			PAIRS ? "nosotros" : "tú", G->score[0], PAIRS ? "ellos" : "el mejor rival",
			PAIRS ? G->score[1] : (G->score[1] > G->score[2] ? (G->score[1] > G->score[3] ? G->score[1] : G->score[3])
			                                                 : (G->score[2] > G->score[3] ? G->score[2] : G->score[3])),
			GN_TUTE_HANDS);
	gfx_text(G->variant == GN_GUINOTE ? "As 11 · Tres 10 · Rey 4 · Sota 3 · Caballo 2"
	                                  : "As 11 · Tres 10 · Rey 4 · Caballo 3 · Sota 2",
		BOTTOM_W / 2, 180, 0, 0.4f, C2D_Color32(200, 230, 200, 255), C2D_AlignCenter);

	bool playing = G->phase == GN_PHASE_PLAYING;
	bool bidding = G->phase == GN_PHASE_BIDDING;
	button_draw(&s_btn_play, s_btn_play.enabled);
	if (playing)
		button_draw(&s_btn_sing, s_btn_sing.enabled);
	button_draw(&s_btn_swap, s_btn_swap.enabled && (playing || bidding));
}

/* Salir a medias desde la pausa: la próxima vez se empieza de cero */
static void scene_leave(void)
{
	s_started[s_variant] = false;
}

const Scene scene_guinote = { guinote_enter, gn_scene_update, gn_scene_draw_top, gn_scene_draw_bottom, scene_leave };
const Scene scene_tute = { tute_enter, gn_scene_update, gn_scene_draw_top, gn_scene_draw_bottom, scene_leave };
const Scene scene_tute_cabron = { cabron_enter, gn_scene_update, gn_scene_draw_top, gn_scene_draw_bottom, scene_leave };
const Scene scene_tute_subastado = { subastado_enter, gn_scene_update, gn_scene_draw_top, gn_scene_draw_bottom, scene_leave };
