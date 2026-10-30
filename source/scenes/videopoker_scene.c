/* Videopóker "Jacks or Better": la máquina de toda la vida. */
#include <math.h>
#include <stdio.h>
#include <time.h>

#include "games/videopoker.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/mii.h"
#include "ui/wallet.h"
#include "ui/widgets.h"

#define CARD_Y  96.0f
#define CARD_SC 0.85f

static VideoPoker s_game;
static bool s_started;
static bool s_on_table; /* hay fichas del monedero en la máquina */
static int s_cursor;
static float s_flash;
static bool s_advised;
static Button s_btn_deal, s_btn_bet, s_btn_max, s_btn_menu;

static int fr_id(PkCard c)
{
	return c.suit * 13 + c.rank - 2;
}

static float card_x(int i)
{
	return BOTTOM_W / 2.0f + (i - 2) * 60.0f;
}

/* Fichas del monedero a la máquina */
static void buy_in(void)
{
	s_game.credits = wallet_take(VP_START);
	s_on_table = true;
}

/* Devuelve al monedero los créditos, también la apuesta de una mano sin
 * acabar (que se anula) */
static void cash_out(void)
{
	if (s_on_table)
		wallet_give(s_game.credits + (s_game.phase == VP_HOLD ? s_game.bet : 0));
	s_game.credits = 0;
	s_on_table = false;
}

static void vp_enter(void)
{
	if (!s_started) {
		vp_init(&s_game, (uint32_t)time(NULL) ^ 0x7E0);
		buy_in();
		s_started = true;
	}
	button_set(&s_btn_menu, 258, 2, 54, 20, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
}

static SceneId vp_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;
	s_flash += dt;
	bool holding = s_game.phase == VP_HOLD;
	static char deal_label[32], bet_label[24];
	snprintf(deal_label, sizeof deal_label, holding ? GLYPH_A " Cambiar" : GLYPH_A " Repartir");
	snprintf(bet_label, sizeof bet_label, GLYPH_X " Apuesta %d", s_game.bet);
	button_set(&s_btn_deal, 216, 205, 100, 28, deal_label, C2D_Color32(40, 140, 60, 255), KEY_A);
	button_set(&s_btn_bet, 4, 205, 100, 28, bet_label, C2D_Color32(60, 90, 160, 255), KEY_X);
	button_set(&s_btn_max, 108, 205, 104, 28, holding ? GLYPH_Y " Consejo" : GLYPH_Y " Apuesta 5",
		C2D_Color32(200, 140, 30, 255), KEY_Y);
	s_btn_deal.enabled = holding || s_game.credits >= s_game.bet;
	s_btn_bet.enabled = !holding;
	s_btn_max.enabled = holding || s_game.credits >= VP_MAX_BET;
	if (holding) {
		if (in->repeat & KEY_LEFT)
			s_cursor = (s_cursor + 4) % 5;
		if (in->repeat & KEY_RIGHT)
			s_cursor = (s_cursor + 1) % 5;
		if (in->down & (KEY_B | KEY_UP | KEY_DOWN)) {
			vp_toggle(&s_game, s_cursor);
			audio_play(SFX_SELECT);
		}
		if (in->touch_down)
			for (int i = 0; i < 5; i++)
				if (fabsf(in->tx - card_x(i)) < CARD_W * CARD_SC / 2 && fabsf(in->ty - CARD_Y) < CARD_H * CARD_SC / 2) {
					s_cursor = i;
					vp_toggle(&s_game, i);
					audio_play(SFX_SELECT);
				}
		if (button_update(&s_btn_max, in)) {
			bool hold[5];
			vp_advice(&s_game, hold);
			for (int i = 0; i < 5; i++)
				s_game.held[i] = hold[i];
			s_advised = true;
			audio_play(SFX_CONFIRM);
		}
		if (button_update(&s_btn_deal, in)) {
			vp_draw(&s_game);
			s_flash = 0.0f;
			audio_play(s_game.win > 0 ? SFX_WIN : SFX_FLIP);
			profile_record_game(GAME_VIDEOPOKER, s_game.win > s_game.bet);
		}
	} else {
		if (button_update(&s_btn_bet, in)) {
			s_game.bet = s_game.bet % VP_MAX_BET + 1;
			audio_play(SFX_CHIP);
		}
		bool max = button_update(&s_btn_max, in);
		if (max)
			s_game.bet = VP_MAX_BET;
		if ((max || button_update(&s_btn_deal, in)) && vp_deal(&s_game)) {
			audio_play(SFX_CARD);
			s_advised = false;
			s_cursor = 0;
		}
		if (s_game.credits < 1 && (in->down & KEY_R)) {
			if (wallet_chips() == 0)
				return SCENE_FICHAS;
			buy_in(); /* más fichas del monedero */
			audio_play(SFX_CHIP);
		}
	}
	return SCENE_VIDEOPOKER;
}

static void vp_draw_top(void)
{
	gfx_background(BG_TABLE_TOP);
	/* Tabla de premios */
	float x0 = 26, y0 = 10;
	gfx_panel(x0 - 8, y0 - 6, 364, 206, DEPTH_UI, C2D_Color32(20, 20, 90, 230), COLOR_GOLD);
	for (int b = 1; b <= VP_MAX_BET; b++) {
		float cx = x0 + 168 + (b - 1) * 38;
		if (b == s_game.bet)
			gfx_rect(cx - 18, y0 - 2, 36, 186, DEPTH_UI, C2D_Color32(200, 40, 40, 160));
		gfx_textf(cx, y0, DEPTH_UI, 0.45f, COLOR_GOLD, C2D_AlignCenter, "%d", b);
	}
	for (int h = 0; h < VP_NOTHING; h++) {
		float y = y0 + 18 + h * 19;
		bool hit = s_game.phase == VP_RESULT && s_game.result == h;
		bool lit = hit && fmodf(s_flash, 0.5f) < 0.3f;
		u32 col = lit ? COLOR_WHITE : hit ? COLOR_GOLD : C2D_Color32(250, 230, 120, 255);
		gfx_text(vp_hand_name(h), x0, y, DEPTH_UI, 0.47f, col, C2D_AlignLeft);
		for (int b = 1; b <= VP_MAX_BET; b++)
			gfx_textf(x0 + 168 + (b - 1) * 38, y, DEPTH_UI, 0.45f, col, C2D_AlignCenter, "%d", vp_pay(h, b));
	}
	gfx_textf(200, 214, DEPTH_UI, 0.55f, COLOR_GOLD, C2D_AlignCenter, "Créditos: %d   ·   Apuesta: %d", s_game.credits,
		s_game.bet);
}

static void vp_draw_bottom(void)
{
	gfx_background(BG_TABLE_BOTTOM);
	button_draw(&s_btn_menu, false);
	bool holding = s_game.phase == VP_HOLD;
	if (s_game.phase == VP_BET && s_game.hands == 0)
		gfx_text("Elige la apuesta y reparte", BOTTOM_W / 2, 40, 0, 0.55f, COLOR_GOLD, C2D_AlignCenter);
	else
		for (int i = 0; i < 5; i++) {
			float x = card_x(i);
			if (holding && i == s_cursor) {
				C2D_ImageTint tint;
				C2D_AlphaImageTint(&tint, 0.7f + 0.3f * sinf(gfx_time() * 6.0f));
				gfx_sprite_ex(SPR_CARD_GLOW, x, CARD_Y, CARD_SC, CARD_SC, 0.0f, 0, &tint);
			}
			gfx_french(fr_id(s_game.hand[i]), x, CARD_Y, CARD_SC, 0.0f, 0, true);
			if (s_game.held[i])
				gfx_text("GUARDADA", x, CARD_Y + 44, 0, 0.42f, COLOR_GOLD, C2D_AlignCenter);
		}
	if (holding)
		gfx_text(s_advised ? "Consejo aplicado: cambia cuando quieras" : "Toca (o " GLYPH_B ") las cartas que guardas",
			BOTTOM_W / 2, 28, 0, 0.45f, COLOR_GOLD, C2D_AlignCenter);
	else if (s_game.phase == VP_RESULT) {
		if (s_game.win > 0)
			gfx_textf(BOTTOM_W / 2, 26, 0, 0.6f, COLOR_GOLD, C2D_AlignCenter, "¡%s! Ganas %d", vp_hand_name(s_game.result),
				s_game.win);
		else
			gfx_text("Sin premio", BOTTOM_W / 2, 28, 0, 0.5f, COLOR_CREAM, C2D_AlignCenter);
	}
	if (s_game.credits < 1 && !holding)
		gfx_text(wallet_chips() > 0 ? "Sin créditos: R para meter más fichas" : "Sin fichas: R para comprar más", BOTTOM_W / 2, 184, 0, 0.45f, COLOR_RED, C2D_AlignCenter);
	button_draw(&s_btn_bet, s_btn_bet.enabled);
	button_draw(&s_btn_max, s_btn_max.enabled);
	button_draw(&s_btn_deal, s_btn_deal.enabled);
}

/* Salir a medias desde la pausa: la próxima vez se empieza de cero */
static void scene_leave(void)
{
	cash_out();
	s_started = false;
}

const Scene scene_videopoker = { vp_enter, vp_update, vp_draw_top, vp_draw_bottom, scene_leave };
