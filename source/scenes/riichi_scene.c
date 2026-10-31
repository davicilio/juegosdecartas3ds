/* Mahjong riichi: la mesa con los ríos de descartes arriba y tu mano abajo. */
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/riichi.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/mii.h"
#include "ui/mjtile.h"
#include "ui/widgets.h"

/* Fichas de los ríos y de las combinaciones de los demás */
#define RIV_W 13.0f
#define RIV_H 17.0f
#define MINI_W 9.0f
#define MINI_H 12.0f
/* Tu mano */
#define HAND_W 21.0f
#define HAND_H 29.0f
#define HAND_Y 150.0f

static const CpuId CPUS[RC_PLAYERS] = { CPU_COUNT, CPU_PACO, CPU_LUCIA, CPU_PEPE };
static const char* const WIND_NAME[4] = { "Este", "Sur", "Oeste", "Norte" };
static const char* const WIND_SHORT[4] = { "E", "S", "O", "N" };

static Riichi s_game;
static bool s_started, s_recorded, s_riichi_mode;
static int s_cursor = -1, s_selected = -1;
static float s_cpu_timer, s_end_time, s_call_flash;
static char s_message[128], s_call_text[16];
static int s_call_player = -1;
static int s_chi_choice;
static Mood s_mood[RC_PLAYERS];
static Button s_btn[5], s_btn_next, s_btn_menu;

static const char* name(int p)
{
	return p == 0 ? profile_get()->avatar.name : cpu_name(CPUS[p]);
}

static const Avatar* avatar(int p)
{
	return p == 0 ? &profile_get()->avatar : cpu_avatar(CPUS[p]);
}

static void set_message(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
static void set_message(const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(s_message, sizeof s_message, fmt, ap);
	va_end(ap);
}

static void shout(int p, const char* text)
{
	s_call_player = p;
	snprintf(s_call_text, sizeof s_call_text, "%s", text);
	s_call_flash = 1.4f;
}

static void hand_start(void)
{
	s_cursor = s_game.hand_n[0] - 1;
	s_selected = -1;
	s_riichi_mode = false;
	s_cpu_timer = 0.0f;
	s_end_time = 0.0f;
	set_message("Este %d%s: reparte %s", s_game.hand_num + 1, s_game.honba ? " (con repetición)" : "",
		s_game.dealer == 0 ? "tú" : name(s_game.dealer));
}

static void new_game(void)
{
	rc_init(&s_game, (uint32_t)time(NULL) ^ 0x7111C);
	s_recorded = false;
	hand_start();
}

static void rc_enter(void)
{
	if (!s_started) {
		s_started = true;
		new_game();
	}
	button_set(&s_btn_menu, 258, 2, 54, 18, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
}

/* Lo que ha pasado al terminar la mano */
static void announce_result(void)
{
	const RcResult* r = &s_game.result;
	s_end_time = 0.0f;
	if (r->draw) {
		set_message("Se acaba el muro: empate");
		audio_play(SFX_SELECT);
		return;
	}
	int w = r->winner;
	shout(w, r->tsumo ? "¡Tsumo!" : "¡Ron!");
	if (w == 0)
		set_message("¡Ganas con %d han! Cobras %d", r->han, r->delta[0]);
	else
		set_message("%s gana con %d han", name(w), r->han);
	audio_play(w == 0 ? SFX_WIN : SFX_LOSE);
	for (int p = 0; p < RC_PLAYERS; p++)
		mood_set(&s_mood[p], p == w ? EXPR_HAPPY : p == r->loser ? EXPR_SAD : EXPR_NORMAL, 4.0f);
}

/* ---------------------------------------------------------------------------
 * Tu turno
 * ------------------------------------------------------------------------- */

static float hand_x(int i)
{
	int n = s_game.hand_n[0];
	bool gap = s_game.current == 0 && s_game.phase == RC_TURN && s_game.drawn >= 0 && i == n - 1;
	return 6 + i * HAND_W + (gap ? 6 : 0);
}

static int touched_tile(const Input* in)
{
	if (!in->touch_down)
		return -1;
	for (int i = 0; i < s_game.hand_n[0]; i++) {
		float x = hand_x(i);
		if (in->tx >= x && in->tx < x + HAND_W && in->ty >= HAND_Y - 12 && in->ty < HAND_Y + HAND_H)
			return i;
	}
	return -1;
}

static void after_discard(int p, bool riichi)
{
	if (riichi) {
		shout(p, "¡Riichi!");
		audio_play(SFX_CHIP);
	} else {
		audio_play(SFX_CARD);
	}
	char t[32];
	rc_tile_name(s_game.discard_tile, t, sizeof t);
	set_message("%s %s %s", p == 0 ? "Tiras" : name(p), p == 0 ? "el" : "tira el", t);
}

static bool my_discard(int i)
{
	bool riichi = s_riichi_mode;
	if (riichi && !rc_riichi_discard_ok(&s_game, i)) {
		audio_play(SFX_ERROR);
		set_message("Con riichi tienes que quedarte a una ficha de ganar");
		return false;
	}
	if (!rc_discard(&s_game, i, riichi)) {
		audio_play(SFX_ERROR);
		if (s_game.riichi_at[0] >= 0)
			set_message("Con riichi tiras la ficha que robas");
		return false;
	}
	after_discard(0, riichi);
	s_riichi_mode = false;
	s_selected = -1;
	return true;
}

static void my_turn(const Input* in)
{
	Riichi* g = &s_game;
	int n = g->hand_n[0];
	const float y = 205, h = 28;
	bool tsumo = rc_can_tsumo(g), riichi = rc_can_riichi(g);
	int kan = rc_kan_option(g);
	button_set(&s_btn[0], 4, y, 76, h, GLYPH_A " Tirar", C2D_Color32(40, 140, 60, 255), KEY_A);
	button_set(&s_btn[1], 84, y, 76, h, GLYPH_Y " Tsumo", C2D_Color32(200, 40, 45, 255), KEY_Y);
	button_set(&s_btn[2], 164, y, 76, h, s_riichi_mode ? GLYPH_X " Sin riichi" : GLYPH_X " Riichi",
		C2D_Color32(200, 140, 30, 255), KEY_X);
	button_set(&s_btn[3], 244, y, 72, h, "Kan " GLYPH_R, C2D_Color32(60, 90, 160, 255), KEY_R);
	s_btn[0].enabled = true;
	s_btn[1].enabled = tsumo;
	s_btn[2].enabled = riichi;
	s_btn[3].enabled = kan >= 0;
	if (s_cursor < 0 || s_cursor >= n)
		s_cursor = n - 1;
	if (g->riichi_at[0] >= 0)
		s_cursor = n - 1; /* con riichi solo se puede tirar la robada */
	if (in->repeat & KEY_LEFT)
		s_cursor = (s_cursor + n - 1) % n;
	if (in->repeat & KEY_RIGHT)
		s_cursor = (s_cursor + 1) % n;
	int t = touched_tile(in);
	if (t >= 0) {
		if (t == s_selected) {
			my_discard(t);
			return;
		}
		s_cursor = s_selected = t;
		audio_play(SFX_SELECT);
	}
	if (button_update(&s_btn[1], in) && rc_tsumo(g)) {
		announce_result();
	} else if (button_update(&s_btn[2], in)) {
		s_riichi_mode = !s_riichi_mode;
		if (s_riichi_mode)
			set_message("Riichi: elige qué ficha tiras (las que te dejan a una de ganar)");
	} else if (button_update(&s_btn[3], in) && rc_self_kan(g, kan)) {
		shout(0, "¡Kan!");
		audio_play(SFX_CHIP);
		s_cursor = g->hand_n[0] - 1;
	} else if (button_update(&s_btn[0], in)) {
		my_discard(s_cursor);
	}
}

/* Tus llamadas sobre el descarte de otro */
static bool my_claim(const Input* in)
{
	Riichi* g = &s_game;
	int opts = rc_claim_options(g, 0);
	if (!opts)
		return false;
	const float y = 205, h = 28;
	int chi[3], nchi = rc_chi_options(g, 0, chi);
	if (s_chi_choice >= nchi)
		s_chi_choice = 0;
	static char chi_label[24];
	if (nchi > 1) {
		char a[8];
		snprintf(a, sizeof a, "%d%d%d", chi[s_chi_choice] % 9 + 1, chi[s_chi_choice] % 9 + 2, chi[s_chi_choice] % 9 + 3);
		snprintf(chi_label, sizeof chi_label, "Chi %s " GLYPH_L, a);
	} else {
		snprintf(chi_label, sizeof chi_label, "Chi " GLYPH_L);
	}
	button_set(&s_btn[0], 4, y, 60, h, GLYPH_Y " Ron", C2D_Color32(200, 40, 45, 255), KEY_Y);
	button_set(&s_btn[1], 68, y, 60, h, GLYPH_X " Pon", C2D_Color32(200, 140, 30, 255), KEY_X);
	button_set(&s_btn[2], 132, y, 56, h, "Kan " GLYPH_R, C2D_Color32(60, 90, 160, 255), KEY_R);
	button_set(&s_btn[3], 192, y, 66, h, chi_label, C2D_Color32(40, 110, 120, 255), KEY_L);
	button_set(&s_btn[4], 262, y, 54, h, GLYPH_B " No", C2D_Color32(120, 120, 120, 255), KEY_B);
	s_btn[0].enabled = opts & RC_CAN_RON;
	s_btn[1].enabled = opts & RC_CAN_PON;
	s_btn[2].enabled = opts & RC_CAN_KAN;
	s_btn[3].enabled = opts & RC_CAN_CHI;
	s_btn[4].enabled = true;
	if (nchi > 1 && (in->down & KEY_DOWN))
		s_chi_choice = (s_chi_choice + 1) % nchi;

	RcClaim mine = RC_PASS;
	bool decided = false;
	if (button_update(&s_btn[0], in))
		mine = RC_DO_RON, decided = true;
	else if (button_update(&s_btn[1], in))
		mine = RC_DO_PON, decided = true;
	else if (button_update(&s_btn[2], in))
		mine = RC_DO_KAN, decided = true;
	else if (button_update(&s_btn[3], in))
		mine = RC_DO_CHI, decided = true;
	else if (button_update(&s_btn[4], in))
		decided = true;
	if (!decided)
		return true;
	RcClaim claims[RC_PLAYERS] = { mine, RC_PASS, RC_PASS, RC_PASS };
	int chi_low = mine == RC_DO_CHI ? chi[s_chi_choice] : -1;
	for (int p = 1; p < RC_PLAYERS; p++)
		if (p != g->discarder) {
			int lo;
			claims[p] = rc_ai_claim(g, p, &lo);
			if (claims[p] == RC_DO_CHI && mine != RC_DO_CHI)
				chi_low = lo;
		}
	int before = g->meld_n[0];
	rc_resolve(g, claims, chi_low);
	if (g->phase == RC_HAND_OVER) {
		announce_result();
	} else if (g->meld_n[0] > before) {
		static const char* const CALL[] = { "¡Chi!", "¡Pon!", "¡Kan!" };
		shout(0, CALL[mine == RC_DO_CHI ? 0 : mine == RC_DO_PON ? 1 : 2]);
		audio_play(SFX_CHIP);
		s_cursor = g->hand_n[0] - 1;
	} else {
		for (int p = 1; p < RC_PLAYERS; p++)
			if (g->current == p && g->meld_n[p] > 0 && g->drawn < 0)
				shout(p, g->melds[p][g->meld_n[p] - 1].type == RC_CHI ? "¡Chi!" : "¡Pon!");
	}
	s_chi_choice = 0;
	return true;
}

/* Llamadas de la CPU cuando tú no puedes llamar */
static void cpu_claims(void)
{
	Riichi* g = &s_game;
	RcClaim claims[RC_PLAYERS] = { RC_PASS, RC_PASS, RC_PASS, RC_PASS };
	int chi_low = -1;
	for (int p = 1; p < RC_PLAYERS; p++)
		if (p != g->discarder) {
			int lo;
			claims[p] = rc_ai_claim(g, p, &lo);
			if (claims[p] == RC_DO_CHI)
				chi_low = lo;
		}
	int melds[RC_PLAYERS];
	for (int p = 0; p < RC_PLAYERS; p++)
		melds[p] = g->meld_n[p];
	rc_resolve(g, claims, chi_low);
	if (g->phase == RC_HAND_OVER) {
		announce_result();
		return;
	}
	for (int p = 1; p < RC_PLAYERS; p++)
		if (g->meld_n[p] > melds[p]) {
			RcMeldType ty = (RcMeldType)g->melds[p][g->meld_n[p] - 1].type;
			shout(p, ty == RC_CHI ? "¡Chi!" : ty == RC_PON ? "¡Pon!" : "¡Kan!");
			audio_play(SFX_CHIP);
		}
}

static void cpu_turn(void)
{
	Riichi* g = &s_game;
	int p = g->current;
	if (rc_ai_tsumo(g)) {
		rc_tsumo(g);
		announce_result();
		return;
	}
	int k = rc_ai_kan(g);
	if (k >= 0 && rc_self_kan(g, k)) {
		shout(p, "¡Kan!");
		audio_play(SFX_CHIP);
		return;
	}
	bool riichi;
	int i = rc_ai_discard(g, &riichi);
	if (rc_discard(g, i, riichi))
		after_discard(p, riichi);
}

static SceneId rc_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;
	Riichi* g = &s_game;
	if (s_call_flash > 0.0f)
		s_call_flash -= dt;
	for (int p = 0; p < RC_PLAYERS; p++)
		mood_update(&s_mood[p], dt);

	switch (g->phase) {
	case RC_TURN:
		if (g->current == 0) {
			my_turn(in);
		} else {
			s_cpu_timer += dt;
			if (s_cpu_timer > 0.7f) {
				s_cpu_timer = 0.0f;
				cpu_turn();
			}
		}
		break;
	case RC_CLAIM:
		if (!my_claim(in)) {
			s_cpu_timer += dt;
			if (s_cpu_timer > 0.35f) {
				s_cpu_timer = 0.0f;
				cpu_claims();
			}
		}
		break;
	case RC_HAND_OVER:
		s_end_time += dt;
		button_set(&s_btn_next, 60, 205, 200, 28, GLYPH_A " Siguiente mano", C2D_Color32(40, 140, 60, 255), KEY_A);
		s_btn_next.enabled = s_end_time > 1.0f;
		if (button_update(&s_btn_next, in)) {
			rc_next(g);
			if (g->phase == RC_GAME_OVER)
				s_end_time = 0.0f;
			else
				hand_start();
		}
		break;
	case RC_GAME_OVER: {
		if (!s_recorded) {
			s_recorded = true;
			int rank = 1;
			for (int p = 1; p < RC_PLAYERS; p++)
				if (g->scores[p] > g->scores[0])
					rank++;
			profile_record_game(GAME_RIICHI, rank == 1);
			audio_play(rank == 1 ? SFX_WIN : SFX_LOSE);
			set_message(rank == 1 ? "¡Primera posición!" : "Quedas en posición %d", rank);
		}
		s_end_time += dt;
		button_set(&s_btn_next, 60, 205, 200, 28, GLYPH_A " Otra partida", C2D_Color32(40, 140, 60, 255), KEY_A);
		s_btn_next.enabled = s_end_time > 1.0f;
		if (button_update(&s_btn_next, in))
			new_game();
		break;
	}
	}
	return SCENE_RIICHI;
}

/* ---------------------------------------------------------------------------
 * Dibujo
 * ------------------------------------------------------------------------- */

/* Río de descartes de p en (x, y), 6 por fila */
static void draw_river(int p, float x, float y)
{
	const Riichi* g = &s_game;
	for (int i = 0; i < g->river_n[p]; i++) {
		float tx = x + (i % 6) * RIV_W, ty = y + (i / 6) * RIV_H;
		if (i / 6 >= 4) {
			tx = x + 6 * RIV_W + (i - 24) % 2 * RIV_W;
			ty = y + (i - 24) / 2 * RIV_H;
		}
		bool last = g->phase == RC_CLAIM && p == g->discarder && i == g->river_n[p] - 1;
		mjtile_riichi(g->river[p][i], tx, ty, RIV_W - 1, RIV_H - 1, DEPTH_CARDS, g->river_taken[p][i]);
		if (i == g->riichi_at[p])
			gfx_rect(tx, ty + RIV_H - 3, RIV_W - 1, 2, DEPTH_CARDS, C2D_Color32(220, 40, 40, 255));
		if (last) {
			float pulse = 0.5f + 0.5f * sinf(gfx_time() * 8.0f);
			u32 c = C2D_Color32(255, 210, 60, (u8)(150 + 100 * pulse));
			gfx_rect(tx - 1, ty - 1, RIV_W + 1, 2, DEPTH_CARDS, c);
			gfx_rect(tx - 1, ty + RIV_H - 3, RIV_W + 1, 2, DEPTH_CARDS, c);
		}
	}
}

/* Combinaciones a la vista de p, en filas de dos */
static void draw_melds_small(int p, float x, float y, float w, float h)
{
	const Riichi* g = &s_game;
	for (int i = 0; i < g->meld_n[p]; i++) {
		const RcMeld* m = &g->melds[p][i];
		float mx = x + (i % 2) * (4 * w + 3), my = y + (i / 2) * (h + 2);
		int n = m->type == RC_CHI || m->type == RC_PON ? 3 : 4;
		for (int k = 0; k < n; k++) {
			int t = m->type == RC_CHI ? m->tile + k : m->tile;
			bool hidden = m->type == RC_KAN_CLOSED && (k == 0 || k == 3);
			mjtile_riichi(hidden ? -1 : t, mx + k * w, my, w - 0.5f, h, DEPTH_CARDS, false);
		}
	}
}

static void draw_seat(int p, float x, float y)
{
	const Riichi* g = &s_game;
	bool turn = g->current == p && (g->phase == RC_TURN || g->phase == RC_CLAIM);
	gfx_panel(x, y, 64, 58, DEPTH_UI, C2D_Color32(10, 50, 25, 220), turn ? COLOR_GOLD : C2D_Color32(20, 70, 35, 255));
	mii_draw(avatar(p), x + 14, y + 16, 10, DEPTH_UI, mood_expr(&s_mood[p]), false);
	int wind = rc_seat_wind(g, p) - RC_EAST;
	gfx_rrect(x + 30, y + 5, 16, 16, 4, DEPTH_UI, wind == 0 ? C2D_Color32(200, 40, 45, 255) : C2D_Color32(60, 60, 80, 255));
	gfx_text(WIND_SHORT[wind], x + 38, y + 5, DEPTH_UI, 0.45f, COLOR_WHITE, C2D_AlignCenter);
	if (g->riichi_at[p] >= 0) {
		gfx_rect(x + 50, y + 8, 10, 3, DEPTH_UI, COLOR_WHITE);
		gfx_ellipse(x + 55, y + 9.5f, 1.5f, 1.5f, DEPTH_UI, COLOR_RED);
	}
	gfx_text(name(p), x + 32, y + 26, DEPTH_UI, 0.42f, COLOR_CREAM, C2D_AlignCenter);
	gfx_textf(x + 32, y + 40, DEPTH_UI, 0.45f, COLOR_GOLD, C2D_AlignCenter, "%d", g->scores[p]);
}

static void draw_center(void)
{
	const Riichi* g = &s_game;
	gfx_panel(158, 82, 84, 76, DEPTH_UI, C2D_Color32(20, 30, 25, 230), C2D_Color32(150, 110, 40, 255));
	gfx_textf(200, 84, DEPTH_UI, 0.5f, COLOR_GOLD, C2D_AlignCenter, "Este %d", g->hand_num + 1);
	gfx_textf(200, 100, DEPTH_UI, 0.42f, COLOR_CREAM, C2D_AlignCenter, "Quedan %d", rc_live_left(g));
	for (int i = 0; i < 5; i++)
		mjtile_riichi(i < g->dora_n ? g->wall[RC_TILES - 6 - 2 * i] : -1, 167 + i * 13, 117, 12, 16, DEPTH_UI, false);
	/* Palitos de riichi en la mesa y repeticiones (honba) */
	gfx_rect(166, 142, 14, 3, DEPTH_UI, COLOR_WHITE);
	gfx_ellipse(173, 143.5f, 1.2f, 1.2f, DEPTH_UI, COLOR_RED);
	gfx_textf(184, 136, DEPTH_UI, 0.4f, COLOR_CREAM, C2D_AlignLeft, "%d", g->sticks);
	gfx_textf(234, 136, DEPTH_UI, 0.4f, COLOR_CREAM, C2D_AlignRight, "Rep %d", g->honba);
}

static void draw_result_panel(void)
{
	const Riichi* g = &s_game;
	const RcResult* r = &g->result;
	if (!banner_draw(s_end_time, 200, 120, 380, 224))
		return;
	float y = 14;
	if (r->draw) {
		gfx_text("Fin del muro: empate", 200, y, DEPTH_BANNER, 0.62f, COLOR_GOLD, C2D_AlignCenter);
		y += 34;
		for (int p = 0; p < RC_PLAYERS; p++, y += 18)
			gfx_textf(200, y, DEPTH_BANNER, 0.48f, COLOR_CREAM, C2D_AlignCenter, "%s: %s  %+d", name(p),
				r->tenpai[p] ? "tenpai" : "noten", r->delta[p]);
		return;
	}
	gfx_textf(200, y, DEPTH_BANNER, 0.6f, COLOR_GOLD, C2D_AlignCenter, "%s: %s%s%s", r->tsumo ? "Tsumo" : "Ron",
		name(r->winner), r->loser >= 0 ? " a " : "", r->loser >= 0 ? name(r->loser) : "");
	y += 24;
	/* Yaku en dos columnas */
	int lines = 0;
	for (int k = 0; k < r->yaku_n; k++, lines++) {
		float x = lines < 5 ? 40 : 210;
		float ly = y + (lines % 5) * 15;
		gfx_text(r->yaku[k].name, x, ly, DEPTH_BANNER, 0.42f, COLOR_CREAM, C2D_AlignLeft);
		if (!r->yakuman)
			gfx_textf(x + 150, ly, DEPTH_BANNER, 0.42f, COLOR_GOLD, C2D_AlignRight, "%d", r->yaku[k].han);
	}
	if (r->dora || r->ura) {
		float x = lines < 5 ? 40 : 210, ly = y + (lines % 5) * 15;
		gfx_textf(x, ly, DEPTH_BANNER, 0.42f, COLOR_CREAM, C2D_AlignLeft, "Dora %d%s", r->dora + r->ura,
			r->ura ? " (con ura)" : "");
	}
	y += 80;
	/* Mano ganadora */
	float x0 = 200 - r->hand_n * 8.0f;
	for (int i = 0; i < r->hand_n; i++)
		mjtile_riichi(r->hand[i], x0 + i * 16, y, 15, 21, DEPTH_BANNER, false);
	const Riichi* gg = g;
	int w = r->winner;
	float mx = x0 + r->hand_n * 16 + 6;
	for (int i = 0; i < gg->meld_n[w]; i++) {
		const RcMeld* m = &gg->melds[w][i];
		int n = m->type == RC_CHI || m->type == RC_PON ? 3 : 4;
		for (int k = 0; k < n; k++)
			mjtile_riichi(m->type == RC_CHI ? m->tile + k : m->tile, mx + k * 10, y + 4, 9.5f, 13, DEPTH_BANNER, false);
		mx += n * 10 + 3;
	}
	y += 28;
	if (r->yakuman)
		gfx_textf(200, y, DEPTH_BANNER, 0.5f, COLOR_GOLD, C2D_AlignCenter, "%s · %d puntos", r->limit, r->points);
	else
		gfx_textf(200, y, DEPTH_BANNER, 0.5f, COLOR_GOLD, C2D_AlignCenter, "%d han %d fu%s%s · %d puntos", r->han, r->fu,
			r->limit ? " · " : "", r->limit ? r->limit : "", r->points);
	y += 22;
	for (int p = 0; p < RC_PLAYERS; p++)
		gfx_textf(52 + p * 99, y, DEPTH_BANNER, 0.42f, r->delta[p] >= 0 ? COLOR_CREAM : C2D_Color32(255, 160, 150, 255),
			C2D_AlignCenter, "%s %+d", name(p), r->delta[p]);
}

static void rc_draw_top(void)
{
	const Riichi* g = &s_game;
	gfx_background(BG_TABLE_TOP);
	draw_river(2, 158, 6);
	draw_river(3, 72, 82);
	draw_river(1, 246, 82);
	draw_river(0, 158, 162);
	draw_center();
	draw_seat(2, 88, 6);
	draw_seat(3, 4, 82);
	draw_seat(1, 332, 82);
	draw_seat(0, 248, 172);
	draw_melds_small(2, 250, 8, MINI_W, MINI_H);
	draw_melds_small(3, 4, 146, MINI_W, MINI_H);
	draw_melds_small(1, 322, 146, MINI_W, MINI_H);
	if (s_call_flash > 0.0f && s_call_player >= 0) {
		static const float CX[4] = { 200, 340, 200, 60 }, CY[4] = { 190, 60, 50, 60 };
		float k = fminf(1.0f, (1.4f - s_call_flash) * 6.0f);
		gfx_panel(CX[s_call_player] - 50, CY[s_call_player] - 16, 100, 32, DEPTH_BANNER, C2D_Color32(200, 40, 45, 240),
			COLOR_GOLD);
		gfx_text(s_call_text, CX[s_call_player], CY[s_call_player] - 12, DEPTH_BANNER, 0.5f + 0.2f * k, COLOR_WHITE,
			C2D_AlignCenter);
	}
	if (g->phase == RC_HAND_OVER)
		draw_result_panel();
	if (g->phase == RC_GAME_OVER && banner_draw(s_end_time, 200, 120, 300, 150)) {
		gfx_text("Fin de la partida", 200, 58, DEPTH_BANNER, 0.62f, COLOR_GOLD, C2D_AlignCenter);
		int order[RC_PLAYERS] = { 0, 1, 2, 3 };
		for (int i = 0; i < RC_PLAYERS; i++)
			for (int j = i + 1; j < RC_PLAYERS; j++)
				if (g->scores[order[j]] > g->scores[order[i]]) {
					int t = order[i];
					order[i] = order[j];
					order[j] = t;
				}
		for (int i = 0; i < RC_PLAYERS; i++)
			gfx_textf(200, 90 + i * 22, DEPTH_BANNER, 0.5f, order[i] == 0 ? COLOR_GOLD : COLOR_CREAM, C2D_AlignCenter,
				"%d.º %s  %d", i + 1, name(order[i]), g->scores[order[i]]);
	}
}

static void rc_draw_bottom(void)
{
	const Riichi* g = &s_game;
	gfx_background(BG_TABLE_BOTTOM);
	gfx_text_wrap(s_message, 8, 2, 0, 0.42f, COLOR_GOLD, 246);
	button_draw(&s_btn_menu, false);
	/* Tus combinaciones */
	float mx = BOTTOM_W - 6;
	for (int i = g->meld_n[0] - 1; i >= 0; i--) {
		const RcMeld* m = &g->melds[0][i];
		int n = m->type == RC_CHI || m->type == RC_PON ? 3 : 4;
		mx -= n * 15 + 4;
		for (int k = 0; k < n; k++) {
			bool hidden = m->type == RC_KAN_CLOSED && (k == 0 || k == 3);
			mjtile_riichi(hidden ? -1 : m->type == RC_CHI ? m->tile + k : m->tile, mx + k * 15, 104, 14.5f, 20, 0, false);
		}
	}
	gfx_textf(8, 34, 0, 0.42f, C2D_Color32(200, 230, 200, 255), C2D_AlignLeft, "Eres %s · %d puntos",
		WIND_NAME[rc_seat_wind(g, 0) - RC_EAST], g->scores[0]);
	/* Esperas cuando estás en tenpai */
	if (g->phase == RC_TURN || g->phase == RC_CLAIM) {
		int c[RC_TYPES];
		rc_counts(g, 0, c);
		int total = g->hand_n[0] + 3 * g->meld_n[0];
		if (total == 13) {
			bool w[RC_TYPES];
			if (rc_waits(c, g->meld_n[0], w) > 0) {
				gfx_text("Esperas:", 8, 52, 0, 0.42f, COLOR_CREAM, C2D_AlignLeft);
				float x = 62;
				bool furiten = false;
				for (int t = 0; t < RC_TYPES; t++)
					if (w[t]) {
						mjtile_riichi(t, x, 52, 11, 15, 0, false);
						x += 12;
						for (int i = 0; i < g->river_n[0]; i++)
							furiten |= g->river[0][i] == t;
					}
				if (furiten)
					gfx_text("furiten", x + 4, 52, 0, 0.42f, COLOR_RED, C2D_AlignLeft);
			}
		}
	}
	/* Tu mano */
	bool my_turn = g->phase == RC_TURN && g->current == 0;
	for (int i = 0; i < g->hand_n[0]; i++) {
		float x = hand_x(i), y = HAND_Y;
		bool lift = my_turn && (i == s_cursor || i == s_selected);
		if (lift)
			y -= 8;
		bool dim = my_turn && s_riichi_mode && !rc_riichi_discard_ok(g, i);
		mjtile_riichi(g->hand[0][i], x, y, HAND_W - 1, HAND_H, 0, dim);
		if (my_turn && i == s_cursor)
			gfx_rect(x, y + HAND_H + 2, HAND_W - 1, 2, 0, COLOR_GOLD);
	}
	if (g->riichi_at[0] >= 0)
		gfx_text("Riichi", 8, 128, 0, 0.45f, COLOR_RED, C2D_AlignLeft);
	if (g->phase == RC_HAND_OVER || g->phase == RC_GAME_OVER) {
		button_draw(&s_btn_next, s_btn_next.enabled);
		return;
	}
	if (my_turn)
		for (int k = 0; k < 4; k++)
			button_draw(&s_btn[k], s_btn[k].enabled);
	else if (g->phase == RC_CLAIM && rc_claim_options(g, 0)) {
		for (int k = 0; k < 5; k++)
			button_draw(&s_btn[k], s_btn[k].enabled);
		char t[32];
		rc_tile_name(g->discard_tile, t, sizeof t);
		gfx_textf(BOTTOM_W / 2, 186, 0, 0.42f, COLOR_GOLD, C2D_AlignCenter, "%s ha tirado el %s", name(g->discarder), t);
	} else {
		gfx_textf(BOTTOM_W / 2, 210, 0, 0.45f, C2D_Color32(200, 230, 200, 255), C2D_AlignCenter, "Juega %s",
			name(g->current));
	}
}

/* Salir a medias desde la pausa: la próxima vez se empieza de cero */
static void scene_leave(void)
{
	s_started = false;
}

const Scene scene_riichi = { rc_enter, rc_update, rc_draw_top, rc_draw_bottom, scene_leave };
