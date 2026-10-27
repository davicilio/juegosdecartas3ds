/* Mesa de la siete y media. */
#include <math.h>
#include <stdio.h>
#include <time.h>

#include "games/sieteymedia.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/widgets.h"

#define CARD_SCALE 0.75f
#define DECK_X 362.0f
#define DECK_Y 66.0f
#define ROW_CX 218.0f
#define BANK_Y 66.0f
#define PLAYER_Y 182.0f
#define MAX_DISCARD 40

static SieteYMedia s_game;
static bool s_started;

static VisCard s_player_vis[SYM_MAX_HAND];
static VisCard s_bank_vis[SYM_MAX_HAND];
static VisCard s_discard[MAX_DISCARD];
static int s_player_shown, s_bank_shown;
static float s_spawn_delay; /* escalonado del reparto */
static float s_bank_timer;
static float s_result_time; /* tiempo desde que se muestra el resultado */
static SymPhase s_prev_phase;

static Button s_btn_left, s_btn_main, s_btn_right, s_btn_alt, s_btn_menu;

static void discard_all(VisCard* vis, int count)
{
	if (count > 0)
		audio_play(SFX_CARD);
	for (int i = 0; i < count; i++) {
		for (int d = 0; d < MAX_DISCARD; d++) {
			if (!s_discard[d].active) {
				s_discard[d] = vis[i];
				s_discard[d].delay = i * 0.04f;
				vis_target(&s_discard[d], -60.0f, 120.0f + (i - count / 2.0f) * 10.0f, CARD_SCALE, false);
				s_discard[d].tangle = -0.6f;
				break;
			}
		}
		vis[i].active = false;
	}
}

static void sync_hand(const SymHand* hand, VisCard* vis, int* shown, float row_y, bool hide_first)
{
	if (hand->count < *shown) {
		discard_all(vis, *shown);
		*shown = 0;
	}
	for (int i = *shown; i < hand->count; i++) {
		vis_spawn(&vis[i], hand->cards[i], DECK_X, DECK_Y, CARD_SCALE, false, s_spawn_delay);
		s_spawn_delay += 0.25f;
	}
	*shown = hand->count;

	float spacing = 34.0f;
	if (hand->count > 1 && spacing * (hand->count - 1) > 190.0f)
		spacing = 190.0f / (hand->count - 1);
	float x0 = ROW_CX - spacing * (hand->count - 1) / 2.0f;
	for (int i = 0; i < hand->count; i++) {
		bool up = !(i == 0 && hide_first);
		vis_target(&vis[i], x0 + i * spacing, row_y, CARD_SCALE, up);
		/* Ligera inclinación aleatoria (pero estable) para que parezca una mesa real */
		vis[i].tangle = ((card_index(hand->cards[i]) * 37) % 9 - 4) * 0.012f;
	}
}

static bool animating(void)
{
	for (int i = 0; i < s_player_shown; i++)
		if (vis_moving(&s_player_vis[i]))
			return true;
	for (int i = 0; i < s_bank_shown; i++)
		if (vis_moving(&s_bank_vis[i]))
			return true;
	return false;
}

static void sym_scene_enter(void)
{
	if (!s_started || s_game.phase == SYM_PHASE_GAME_OVER) {
		sym_init(&s_game, (uint32_t)time(NULL) ^ 0x5EED7);
		s_started = true;
		s_player_shown = s_bank_shown = 0;
	}
	s_prev_phase = s_game.phase;
	button_set(&s_btn_menu, 258, 10, 54, 20, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
}

static void layout_buttons(void)
{
	const float y = 205, h = 28;
	switch (s_game.phase) {
	case SYM_PHASE_BETTING:
		button_set(&s_btn_left, 12, y, 56, h, "- " GLYPH_L, C2D_Color32(60, 90, 160, 255), KEY_L);
		button_set(&s_btn_main, 80, y, 160, h, GLYPH_A " Repartir", C2D_Color32(40, 140, 60, 255), KEY_A);
		button_set(&s_btn_right, 252, y, 56, h, "+ " GLYPH_R, C2D_Color32(60, 90, 160, 255), KEY_R);
		break;
	case SYM_PHASE_PLAYER:
		button_set(&s_btn_main, 12, y, 144, h, GLYPH_A " Pedir carta", C2D_Color32(40, 140, 60, 255), KEY_A);
		button_set(&s_btn_alt, 164, y, 144, h, GLYPH_B " Plantarse", C2D_Color32(190, 60, 50, 255), KEY_B);
		break;
	case SYM_PHASE_ROUND_OVER:
		button_set(&s_btn_main, 60, y, 200, h, GLYPH_A " Siguiente mano", C2D_Color32(40, 140, 60, 255), KEY_A);
		break;
	case SYM_PHASE_GAME_OVER:
		button_set(&s_btn_main, 12, y, 144, h, GLYPH_A " Nueva partida", C2D_Color32(40, 140, 60, 255), KEY_A);
		button_set(&s_btn_alt, 164, y, 144, h, GLYPH_B " Menú", C2D_Color32(190, 60, 50, 255), KEY_B);
		break;
	default:
		break;
	}
}

static SceneId sym_scene_update(const Input* in, float dt)
{
	SymPhase phase = s_game.phase;
	bool busy = animating();
	bool result_ready = (phase == SYM_PHASE_ROUND_OVER || phase == SYM_PHASE_GAME_OVER) && !busy;

	layout_buttons();
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;

	switch (phase) {
	case SYM_PHASE_BETTING:
		if (button_update(&s_btn_left, in) || (in->repeat & KEY_LEFT)) {
			sym_change_bet(&s_game, -SYM_BET_STEP);
			audio_play(SFX_CHIP);
		}
		if (button_update(&s_btn_right, in) || (in->repeat & KEY_RIGHT)) {
			sym_change_bet(&s_game, SYM_BET_STEP);
			audio_play(SFX_CHIP);
		}
		if (button_update(&s_btn_main, in))
			sym_deal(&s_game);
		break;
	case SYM_PHASE_PLAYER:
		if (busy)
			break;
		if (button_update(&s_btn_main, in))
			sym_hit(&s_game);
		else if (button_update(&s_btn_alt, in))
			sym_stand(&s_game);
		break;
	case SYM_PHASE_BANK:
		s_bank_timer += dt;
		if (!busy && s_bank_timer > 0.7f) {
			s_bank_timer = 0.0f;
			sym_bank_step(&s_game);
		}
		break;
	case SYM_PHASE_ROUND_OVER:
		if (result_ready && s_result_time > 0.4f && button_update(&s_btn_main, in))
			sym_next_round(&s_game);
		break;
	case SYM_PHASE_GAME_OVER:
		if (result_ready && s_result_time > 0.4f) {
			if (button_update(&s_btn_main, in)) {
				sym_init(&s_game, (uint32_t)time(NULL));
			} else if (button_update(&s_btn_alt, in)) {
				return SCENE_MENU;
			}
		}
		break;
	}

	if (s_game.phase != s_prev_phase) {
		s_bank_timer = 0.0f;
		s_result_time = 0.0f;
		s_prev_phase = s_game.phase;
	}
	if (result_ready) {
		if (s_result_time == 0.0f)
			audio_play(s_game.last_delta > 0 ? SFX_WIN : SFX_LOSE);
		s_result_time += dt;
	}

	bool hide_bank = s_game.phase == SYM_PHASE_PLAYER || s_game.phase == SYM_PHASE_BETTING;
	if (s_spawn_delay > 0.0f)
		s_spawn_delay -= dt;
	if (s_spawn_delay < 0.0f)
		s_spawn_delay = 0.0f;
	sync_hand(&s_game.player, s_player_vis, &s_player_shown, PLAYER_Y, false);
	sync_hand(&s_game.bank, s_bank_vis, &s_bank_shown, BANK_Y, hide_bank);

	for (int i = 0; i < s_player_shown; i++)
		vis_update(&s_player_vis[i], dt);
	for (int i = 0; i < s_bank_shown; i++)
		vis_update(&s_bank_vis[i], dt);
	for (int d = 0; d < MAX_DISCARD; d++) {
		vis_update(&s_discard[d], dt);
		if (s_discard[d].active && !vis_moving(&s_discard[d]))
			s_discard[d].active = false;
	}
	return SCENE_SIETEYMEDIA;
}

static void draw_value_badge(const char* who, float y, int halves, bool known, bool active)
{
	char value[16];
	if (known)
		sym_format_value(halves, value, sizeof value);
	else
		snprintf(value, sizeof value, "?");
	u32 border = active ? COLOR_GOLD : C2D_Color32(20, 60, 30, 255);
	gfx_panel(18, y - 30, 70, 60, DEPTH_UI, C2D_Color32(10, 50, 25, 200), border);
	gfx_text(who, 53, y - 25, DEPTH_UI, 0.45f, COLOR_CREAM, C2D_AlignCenter);
	u32 col = halves > SYM_LIMIT && known ? COLOR_RED : halves == SYM_LIMIT && known ? COLOR_GOLD : COLOR_WHITE;
	gfx_text(value, 53, y - 9, DEPTH_UI, 0.9f, col, C2D_AlignCenter);
}

static void draw_banner(void)
{
	const char* title = NULL;
	u32 color = COLOR_WHITE;
	switch (s_game.outcome) {
	case SYM_OUTCOME_PLAYER_BUST:
		title = "¡Te has pasado!";
		color = COLOR_RED;
		break;
	case SYM_OUTCOME_BANK_BUST:
		title = "¡La banca se pasa!";
		color = COLOR_GREEN;
		break;
	case SYM_OUTCOME_PLAYER_WINS:
		title = "¡Ganas la mano!";
		color = COLOR_GREEN;
		break;
	case SYM_OUTCOME_BANK_WINS:
		title = "Gana la banca";
		color = COLOR_RED;
		break;
	case SYM_OUTCOME_REAL:
		title = "¡Siete y media real!";
		color = COLOR_GOLD;
		break;
	default:
		return;
	}
	if (!banner_draw(s_result_time, 200, 124, 320, 42))
		return;
	gfx_text(title, 200, 103, DEPTH_BANNER, 0.7f, color, C2D_AlignCenter);
	if (s_game.phase == SYM_PHASE_GAME_OVER)
		gfx_text("Te has quedado sin fichas", 200, 126, DEPTH_BANNER, 0.45f, COLOR_CREAM, C2D_AlignCenter);
	else
		gfx_textf(200, 126, DEPTH_BANNER, 0.45f, s_game.last_delta > 0 ? COLOR_GOLD : COLOR_CREAM,
			C2D_AlignCenter, "%+d fichas", s_game.last_delta);
}

static void sym_scene_draw_top(void)
{
	gfx_background(BG_TABLE_TOP);

	/* Mazo con algo de grosor */
	for (int i = 0; i < 5; i++)
		gfx_card(card_make(SUIT_OROS, 1), DECK_X - i * 0.6f, DECK_Y - i * 0.8f, CARD_SCALE, 0.0f, 0.0f,
			DEPTH_CARDS + i * 0.1f, 0.0f, i == 0);

	bool bank_known = !(s_game.phase == SYM_PHASE_PLAYER || s_game.phase == SYM_PHASE_BETTING);
	draw_value_badge("BANCA", BANK_Y, sym_hand_halves(&s_game.bank), bank_known && s_game.bank.count > 0,
		s_game.phase == SYM_PHASE_BANK);
	draw_value_badge("TÚ", PLAYER_Y, sym_hand_halves(&s_game.player), s_game.player.count > 0,
		s_game.phase == SYM_PHASE_PLAYER);

	for (int d = 0; d < MAX_DISCARD; d++)
		vis_draw(&s_discard[d]);
	for (int i = 0; i < s_bank_shown; i++)
		vis_draw(&s_bank_vis[i]);
	for (int i = 0; i < s_player_shown; i++)
		vis_draw(&s_player_vis[i]);

	if (s_game.phase == SYM_PHASE_BETTING)
		gfx_text("Haz tu apuesta", ROW_CX, 110, DEPTH_UI, 0.7f, COLOR_CREAM, C2D_AlignCenter);
	if ((s_game.phase == SYM_PHASE_ROUND_OVER || s_game.phase == SYM_PHASE_GAME_OVER) && !animating())
		draw_banner();
}

static void sym_scene_draw_bottom(void)
{
	char value[16];
	gfx_background(BG_TABLE_BOTTOM);

	/* Fichas y apuesta */
	gfx_text("Fichas", 20, 14, 0, 0.5f, COLOR_CREAM, C2D_AlignLeft);
	gfx_textf(20, 30, 0, 0.8f, COLOR_GOLD, C2D_AlignLeft, "%d", s_game.chips);
	gfx_chip_stack(s_game.chips, 120, 52, 0);

	gfx_panel(80, 82, 160, 62, 0, C2D_Color32(0, 40, 15, 170), C2D_Color32(200, 160, 60, 255));
	gfx_text("Apuesta", 160, 86, 0, 0.5f, COLOR_CREAM, C2D_AlignCenter);
	gfx_textf(160, 102, 0, 0.9f, COLOR_WHITE, C2D_AlignCenter, "%d", s_game.bet);
	gfx_chip_stack(s_game.bet, 270, 120, 0);

	const char* hint = "";
	char buf[96];
	switch (s_game.phase) {
	case SYM_PHASE_BETTING:
		hint = "Elige la apuesta con " GLYPH_L "/" GLYPH_R " o la cruceta";
		break;
	case SYM_PHASE_PLAYER:
		sym_format_value(sym_hand_halves(&s_game.player), value, sizeof value);
		snprintf(buf, sizeof buf, "Llevas %s. ¿Pides otra carta?", value);
		hint = buf;
		break;
	case SYM_PHASE_BANK:
		hint = "La banca está jugando...";
		break;
	case SYM_PHASE_ROUND_OVER:
	case SYM_PHASE_GAME_OVER:
		snprintf(buf, sizeof buf, "Manos ganadas: %d de %d", s_game.rounds_won, s_game.rounds_played);
		hint = buf;
		break;
	}
	gfx_text(hint, BOTTOM_W / 2, 156, 0, 0.5f, COLOR_CREAM, C2D_AlignCenter);
	gfx_text("Siete y media real (7 + figura) paga doble", BOTTOM_W / 2, 178, 0, 0.4f,
		C2D_Color32(200, 230, 200, 255), C2D_AlignCenter);

	button_draw(&s_btn_menu, false);
	bool ready = !animating();
	switch (s_game.phase) {
	case SYM_PHASE_BETTING:
		button_draw(&s_btn_left, false);
		button_draw(&s_btn_main, true);
		button_draw(&s_btn_right, false);
		break;
	case SYM_PHASE_PLAYER:
		s_btn_main.enabled = s_btn_alt.enabled = ready;
		button_draw(&s_btn_main, ready);
		button_draw(&s_btn_alt, false);
		break;
	case SYM_PHASE_ROUND_OVER:
		button_draw(&s_btn_main, ready);
		break;
	case SYM_PHASE_GAME_OVER:
		button_draw(&s_btn_main, ready);
		button_draw(&s_btn_alt, false);
		break;
	default:
		break;
	}
}

const Scene scene_sieteymedia = { sym_scene_enter, sym_scene_update, sym_scene_draw_top, sym_scene_draw_bottom };
