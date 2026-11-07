/* Keno: el cartón de 80 números abajo y el sorteo arriba. */
#include <math.h>
#include <stdio.h>
#include <time.h>

#include "games/keno.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/mii.h"
#include "ui/wallet.h"
#include "ui/widgets.h"

#define GRID_X 10.0f
#define GRID_Y 24.0f
#define CW     30.0f
#define CH     21.0f

static const int BETS[] = { 1, 2, 5, 10, 25, 50, 100 };
#define NBETS ((int)(sizeof BETS / sizeof BETS[0]))

typedef enum { PH_PICK, PH_DRAW, PH_RESULT } Phase;

static Keno s_game;
static bool s_started;
static Phase s_phase;
static int s_bet_idx = 2, s_paid, s_win, s_shown;
static float s_t, s_result_time;
static char s_message[80];
static Button s_btn_play, s_btn_clear, s_btn_quick, s_btn_menu;

static void kn_enter(void)
{
	if (!s_started) {
		s_started = true;
		kn_init(&s_game, (uint32_t)time(NULL) ^ 0x4E40);
		s_phase = PH_PICK;
		s_paid = 0;
		snprintf(s_message, sizeof s_message, "Marca de 1 a 10 números");
	}
	button_set(&s_btn_menu, 262, 2, 54, 18, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
}

static int bet(void)
{
	return BETS[s_bet_idx];
}

static void reset_draw(void)
{
	for (int i = 0; i <= KN_NUMBERS; i++)
		s_game.is_drawn[i] = false;
	s_game.hits = 0;
}

static SceneId kn_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;
	bool picking = s_phase != PH_DRAW;
	button_set(&s_btn_clear, 4, 205, 96, 28, GLYPH_X " Borrar", C2D_Color32(150, 60, 50, 255), KEY_X);
	button_set(&s_btn_quick, 104, 205, 100, 28, GLYPH_Y " Al azar", C2D_Color32(60, 90, 160, 255), KEY_Y);
	button_set(&s_btn_play, 208, 205, 108, 28, GLYPH_A " Jugar", C2D_Color32(40, 140, 60, 255), KEY_A);
	s_btn_clear.enabled = picking && s_game.picks > 0;
	s_btn_quick.enabled = picking;
	s_btn_play.enabled = picking && s_game.picks > 0;
	if (s_phase == PH_DRAW) {
		s_t += dt;
		while (s_shown < KN_DRAWN && s_t > 0.22f * (s_shown + 1)) {
			int n = s_game.drawn[s_shown++];
			audio_play(s_game.picked[n] ? SFX_CHIP : SFX_SELECT);
		}
		if (s_shown >= KN_DRAWN && s_t > 0.22f * KN_DRAWN + 0.5f) {
			s_win = kn_payout(s_paid, s_game.picks, s_game.hits);
			if (s_win)
				wallet_give(s_win);
			profile_record_game(GAME_KENO, s_win > s_paid);
			audio_play(s_win > s_paid ? SFX_WIN : s_win ? SFX_CHIP : SFX_LOSE);
			snprintf(s_message, sizeof s_message, "%d acierto%s: %s", s_game.hits, s_game.hits == 1 ? "" : "s",
				s_win ? "¡premio!" : "sin premio");
			s_paid = 0;
			s_phase = PH_RESULT;
			s_result_time = 0.0f;
		}
		return SCENE_KENO;
	}
	s_result_time += dt;
	if (in->down & (KEY_L | KEY_R)) {
		s_bet_idx = (s_bet_idx + ((in->down & KEY_R) ? 1 : NBETS - 1)) % NBETS;
		audio_play(SFX_CHIP);
	}
	if (in->touch_down) {
		int c = (int)floorf((in->tx - GRID_X) / CW), r = (int)floorf((in->ty - GRID_Y) / CH);
		if (c >= 0 && c < 10 && r >= 0 && r < 8) {
			if (s_phase == PH_RESULT) {
				reset_draw();
				s_phase = PH_PICK;
			}
			if (kn_toggle(&s_game, r * 10 + c + 1))
				audio_play(SFX_SELECT);
			else
				audio_play(SFX_ERROR);
		}
		/* Apuesta: tocar la cifra la cambia */
		if (input_touch_in(in, 200, 186, 116, 16)) {
			s_bet_idx = (s_bet_idx + 1) % NBETS;
			audio_play(SFX_CHIP);
		}
	}
	if (button_update(&s_btn_clear, in)) {
		kn_clear(&s_game);
		s_phase = PH_PICK;
		audio_play(SFX_SELECT);
	}
	if (button_update(&s_btn_quick, in)) {
		reset_draw();
		kn_quick_pick(&s_game, s_game.picks ? s_game.picks : 5);
		s_phase = PH_PICK;
		audio_play(SFX_FLIP);
	}
	if (button_update(&s_btn_play, in)) {
		if (wallet_chips() < bet()) {
			if (wallet_chips() == 0)
				return SCENE_FICHAS;
			audio_play(SFX_ERROR);
			snprintf(s_message, sizeof s_message, "No tienes %d fichas: baja la apuesta con L/R", bet());
		} else {
			s_paid = wallet_take(bet());
			kn_draw(&s_game);
			s_shown = 0;
			s_t = 0.0f;
			s_win = 0;
			s_phase = PH_DRAW;
			snprintf(s_message, sizeof s_message, "Salen las bolas...");
		}
	}
	return SCENE_KENO;
}

static bool drawn_so_far(int n)
{
	for (int i = 0; i < s_shown; i++)
		if (s_game.drawn[i] == n)
			return true;
	return false;
}

static void kn_draw_top(void)
{
	gfx_background(BG_TABLE_TOP);
	/* Bolas sorteadas */
	gfx_panel(10, 8, 240, 150, DEPTH_UI, C2D_Color32(15, 25, 50, 220), COLOR_GOLD);
	gfx_text("Bolas", 130, 12, DEPTH_UI, 0.5f, COLOR_GOLD, C2D_AlignCenter);
	int n = s_phase == PH_PICK ? 0 : s_shown;
	for (int i = 0; i < n; i++) {
		int v = s_game.drawn[i];
		float x = 32 + (i % 5) * 46, y = 44 + (i / 5) * 28;
		bool hit = s_game.picked[v];
		gfx_ellipse(x, y, 12, 12, DEPTH_CARDS, hit ? C2D_Color32(240, 190, 30, 255) : C2D_Color32(230, 230, 235, 255));
		gfx_ellipse(x - 3, y - 4, 4, 3, DEPTH_CARDS, C2D_Color32(255, 255, 255, 180));
		gfx_textf_plain(x, y - 7, DEPTH_CARDS, 0.45f, C2D_Color32(30, 30, 40, 255), C2D_AlignCenter, "%d", v);
	}
	/* Tabla de premios para los números elegidos */
	gfx_panel(262, 8, 130, 224, DEPTH_UI, C2D_Color32(40, 10, 15, 220), COLOR_GOLD);
	gfx_textf(327, 12, DEPTH_UI, 0.45f, COLOR_GOLD, C2D_AlignCenter, "Premios (%d)", s_game.picks);
	gfx_text("aciertos  cobras", 327, 30, DEPTH_UI, 0.38f, COLOR_CREAM, C2D_AlignCenter);
	int row = 0;
	for (int h = s_game.picks; h >= 0 && s_game.picks; h--) {
		int pay = kn_payout(bet(), s_game.picks, h);
		if (!pay)
			continue;
		float y = 48 + row++ * 17;
		bool lit = s_phase == PH_RESULT && h == s_game.hits && fmodf(gfx_time(), 0.6f) < 0.35f;
		u32 c = lit ? COLOR_WHITE : COLOR_GOLD;
		gfx_textf(290, y, DEPTH_UI, 0.45f, c, C2D_AlignCenter, "%d", h);
		gfx_textf(380, y, DEPTH_UI, 0.45f, c, C2D_AlignRight, "%d", pay);
	}
	gfx_panel(10, 166, 240, 66, DEPTH_UI, C2D_Color32(15, 25, 50, 220), COLOR_GOLD);
	gfx_textf(130, 172, DEPTH_UI, 0.5f, COLOR_CREAM, C2D_AlignCenter, "Aciertos: %d de %d", s_phase == PH_PICK ? 0 : s_game.hits,
		s_game.picks);
	if (s_phase == PH_RESULT)
		gfx_textf(130, 196, DEPTH_UI, 0.62f, COLOR_GOLD, C2D_AlignCenter, s_win ? "Cobras %d" : "Sin premio", s_win);
	else
		gfx_textf(130, 198, DEPTH_UI, 0.45f, COLOR_CREAM, C2D_AlignCenter, "Fichas: %d", wallet_chips());
}

static void kn_draw_bottom(void)
{
	gfx_background(BG_TABLE_BOTTOM);
	gfx_text(s_message, 6, 4, 0, 0.45f, COLOR_GOLD, C2D_AlignLeft);
	button_draw(&s_btn_menu, false);
	for (int i = 0; i < KN_NUMBERS; i++) {
		int v = i + 1;
		float x = GRID_X + (i % 10) * CW, y = GRID_Y + (i / 10) * CH;
		bool picked = s_game.picked[v], drawn = s_phase != PH_PICK && drawn_so_far(v);
		u32 fill = picked && drawn ? C2D_Color32(240, 190, 30, 255)
			: picked             ? C2D_Color32(200, 40, 45, 255)
			: drawn              ? C2D_Color32(120, 150, 200, 255)
			                     : C2D_Color32(250, 246, 235, 255);
		gfx_rrect(x + 1, y + 1, CW - 2, CH - 2, 4, 0, fill);
		gfx_textf_plain(x + CW / 2, y + 3, 0, 0.42f, picked ? COLOR_WHITE : C2D_Color32(60, 40, 30, 255), C2D_AlignCenter,
			"%d", v);
	}
	gfx_textf(10, 188, 0, 0.45f, COLOR_CREAM, C2D_AlignLeft, "Elegidos: %d/10", s_game.picks);
	gfx_textf(310, 188, 0, 0.45f, COLOR_GOLD, C2D_AlignRight, GLYPH_L GLYPH_R " Apuesta: %d", bet());
	button_draw(&s_btn_clear, false);
	button_draw(&s_btn_quick, false);
	button_draw(&s_btn_play, s_btn_play.enabled);
}

static void scene_leave(void)
{
	/* Salir durante el sorteo lo anula */
	if (s_phase == PH_DRAW && s_paid)
		wallet_give(s_paid);
	s_paid = 0;
	s_started = false;
}

const Scene scene_keno = { kn_enter, kn_update, kn_draw_top, kn_draw_bottom, scene_leave };
