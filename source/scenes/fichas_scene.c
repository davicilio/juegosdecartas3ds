/* Fichas: el monedero común a todos los juegos con apuestas. Se compran con
 * las monedas de juego de la consola, las que se ganan andando con la 3DS. */
#include <stdio.h>

#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/wallet.h"
#include "ui/widgets.h"

static int s_coins; /* monedas de juego, o -1 si no se pueden leer */
static int s_buy;   /* monedas que se van a cambiar */
static Button s_btn_less, s_btn_more, s_btn_buy, s_btn_done;
static char s_msg[96];
static float s_flash;

static void clamp_buy(void)
{
	if (s_buy > s_coins)
		s_buy = s_coins;
	if (s_buy < 1)
		s_buy = 1;
}

static void fichas_enter(void)
{
	s_coins = playcoins_get();
	s_buy = 1;
	s_flash = 0.0f;
	if (s_coins < 0)
		snprintf(s_msg, sizeof s_msg, "No se pueden leer las monedas de juego de la consola.");
	else if (s_coins == 0)
		snprintf(s_msg, sizeof s_msg, "No tienes monedas de juego. ¡Sal a andar con la consola!");
	else
		snprintf(s_msg, sizeof s_msg, "Cada moneda de juego vale %d fichas.", CHIPS_PER_COIN);
	clamp_buy();
	button_set(&s_btn_less, 40, 110, 50, 34, "-", C2D_Color32(120, 30, 35, 255), KEY_DLEFT);
	button_set(&s_btn_more, 230, 110, 50, 34, "+", C2D_Color32(40, 140, 60, 255), KEY_DRIGHT);
	button_set(&s_btn_buy, 12, 205, 160, 28, GLYPH_A " Cambiar", C2D_Color32(200, 140, 30, 255), KEY_A);
	button_set(&s_btn_done, 180, 205, 128, 28, GLYPH_B " Volver", C2D_Color32(40, 140, 60, 255), KEY_B);
}

static SceneId fichas_update(const Input* in, float dt)
{
	if (s_flash > 0.0f)
		s_flash -= dt;
	bool can = s_coins > 0;
	s_btn_less.enabled = can && s_buy > 1;
	s_btn_more.enabled = can && s_buy < s_coins;
	s_btn_buy.enabled = can;
	if (can) {
		if (in->repeat & (KEY_R | KEY_DUP))
			s_buy += 10;
		if (in->repeat & (KEY_L | KEY_DDOWN))
			s_buy -= 10;
		clamp_buy();
	}
	if (button_update(&s_btn_less, in)) {
		s_buy--;
		clamp_buy();
		audio_play(SFX_SELECT);
	} else if (button_update(&s_btn_more, in)) {
		s_buy++;
		clamp_buy();
		audio_play(SFX_SELECT);
	} else if (button_update(&s_btn_buy, in)) {
		if (playcoins_spend(s_buy)) {
			wallet_give(s_buy * CHIPS_PER_COIN);
			audio_play(SFX_WIN);
			s_flash = 1.0f;
			snprintf(s_msg, sizeof s_msg, "¡Hecho! %d monedas por %d fichas.", s_buy, s_buy * CHIPS_PER_COIN);
		} else {
			audio_play(SFX_ERROR);
			snprintf(s_msg, sizeof s_msg, "No se han podido gastar las monedas de juego.");
		}
		s_coins = playcoins_get();
		clamp_buy();
	} else if (button_update(&s_btn_done, in) || (in->down & KEY_START)) {
		return SCENE_MENU;
	}
	return SCENE_FICHAS;
}

static void fichas_draw_top(void)
{
	gfx_background(BG_TITLE_TOP);
	gfx_panel(60, 20, 280, 200, DEPTH_UI, C2D_Color32(40, 8, 12, 210), C2D_Color32(230, 170, 50, 255));
	gfx_text("Tus fichas", 200, 30, DEPTH_UI, 0.7f, COLOR_GOLD, C2D_AlignCenter);
	/* Montones de fichas: más cuantas más tengas */
	static const int SPR[4] = { SPR_CHIP_BLACK, SPR_CHIP_GREEN, SPR_CHIP_BLUE, SPR_CHIP_RED };
	int chips = wallet_chips();
	int piles = chips <= 0 ? 0 : chips < 500 ? 1 : chips < 2000 ? 2 : chips < 5000 ? 3 : chips < 10000 ? 4 : 5;
	for (int k = 0; k < piles; k++) {
		int n = 6 + (k * 7) % 5;
		float x = 200 + (k - (piles - 1) / 2.0f) * 34.0f;
		for (int j = 0; j < n; j++)
			gfx_sprite_ex(SPR[k % 4], x, 112 - j * 4.0f, 1.0f, 1.0f, 0.0f, DEPTH_UI + j * 0.05f, NULL);
	}
	float pop = s_flash > 0.0f ? 0.2f * s_flash : 0.0f;
	gfx_textf(200, 128, DEPTH_UI, 0.9f + pop, COLOR_CREAM, C2D_AlignCenter, "%d", chips);
	gfx_text_wrap("Valen para todos los juegos con apuestas. Al sentarte a la mesa coges fichas de aquí, y al "
	              "acabar (o salir) vuelven las que te queden.",
		76, 160, DEPTH_UI, 0.42f, C2D_Color32(230, 215, 190, 255), 250);
}

static void fichas_draw_bottom(void)
{
	gfx_background(BG_MENU_BOTTOM);
	gfx_text("Monedas de juego", BOTTOM_W / 2, 10, 0, 0.55f, COLOR_GOLD, C2D_AlignCenter);
	if (s_coins >= 0)
		gfx_textf(BOTTOM_W / 2, 32, 0, 0.6f, COLOR_CREAM, C2D_AlignCenter, "Tienes %d", s_coins);
	gfx_text_wrap(s_msg, 24, 60, 0, 0.45f, C2D_Color32(230, 215, 190, 255), 272);
	if (s_coins > 0) {
		button_draw(&s_btn_less, false);
		button_draw(&s_btn_more, false);
		gfx_textf(BOTTOM_W / 2, 110, 0, 0.6f, COLOR_CREAM, C2D_AlignCenter, s_buy == 1 ? "%d moneda" : "%d monedas", s_buy);
		gfx_textf(BOTTOM_W / 2, 132, 0, 0.5f, COLOR_GOLD, C2D_AlignCenter, "= %d fichas", s_buy * CHIPS_PER_COIN);
		gfx_text("L/R: de 10 en 10", BOTTOM_W / 2, 170, 0, 0.4f, C2D_Color32(200, 180, 170, 255), C2D_AlignCenter);
		button_draw(&s_btn_buy, true);
	}
	button_draw(&s_btn_done, s_coins <= 0);
}

const Scene scene_fichas = { fichas_enter, fichas_update, fichas_draw_top, fichas_draw_bottom };
