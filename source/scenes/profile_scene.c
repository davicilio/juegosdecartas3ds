/* Perfil del jugador: su Mii de la consola y sus estadísticas. */
#include <math.h>
#include <stdio.h>

#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/mii.h"
#include "ui/miiface.h"
#include "ui/widgets.h"

static Mood s_mood;
static Button s_btn_pick, s_btn_retry, s_btn_rivals, s_btn_done;
static bool s_welcome;

static void profile_enter(void)
{
	s_welcome = !profile_get()->created;
	mood_set(&s_mood, EXPR_HAPPY, 1.5f);
	button_set(&s_btn_pick, 40, 20, 240, 40, GLYPH_A " Elegir mi Mii", C2D_Color32(40, 140, 60, 255), KEY_A);
	button_set(&s_btn_retry, 70, 132, 180, 28, GLYPH_Y " Reintentar descarga", C2D_Color32(200, 140, 30, 255), KEY_Y);
	button_set(&s_btn_rivals, 12, 205, 140, 28, GLYPH_X " Tus rivales", C2D_Color32(60, 90, 160, 255), KEY_X);
	button_set(&s_btn_done, 160, 205, 148, 28, s_welcome ? "¡A jugar!" : GLYPH_B " Listo", C2D_Color32(40, 140, 60, 255),
		s_welcome ? 0 : KEY_B);
}

static SceneId finish(void)
{
	PlayerProfile* p = profile_get();
	p->created = 1;
	profile_store();
	return SCENE_MENU;
}

static bool has_error(void)
{
	return miiface_last_error()[0] != '\0';
}

static SceneId profile_update(const Input* in, float dt)
{
	mood_update(&s_mood, dt);
	s_btn_retry.enabled = has_error();

	if (button_update(&s_btn_pick, in)) {
		if (profile_pick_console_mii()) {
			audio_play(SFX_WIN);
			mood_set(&s_mood, EXPR_HAPPY, 2.0f);
		}
	} else if (button_update(&s_btn_retry, in)) {
		miiface_retry();
	} else if (button_update(&s_btn_rivals, in)) {
		finish();
		return SCENE_RIVALS;
	} else if (button_update(&s_btn_done, in) || (!s_welcome && (in->down & KEY_START))) {
		return finish();
	}
	return SCENE_PROFILE;
}

static void profile_draw_top(void)
{
	const PlayerProfile* p = profile_get();
	gfx_background(BG_TITLE_TOP);

	/* Tu Mii de cuerpo entero sobre un pedestal */
	gfx_ellipse(110, 196, 56, 10, DEPTH_TABLE, C2D_Color32(0, 0, 0, 90));
	mii_draw_body(&p->avatar, 110, 198, 182, DEPTH_BANNER, mood_expr(&s_mood));
	gfx_panel(30, 200, 160, 28, DEPTH_UI, C2D_Color32(110, 20, 25, 230), COLOR_GOLD);
	gfx_text(p->avatar.official ? p->avatar.name : "Sin Mii", 110, 204, DEPTH_UI, 0.55f, COLOR_CREAM, C2D_AlignCenter);

	/* Estadísticas */
	gfx_panel(210, 16, 178, 208, DEPTH_UI, C2D_Color32(40, 8, 12, 210), C2D_Color32(230, 170, 50, 255));
	gfx_text(s_welcome ? "¡Bienvenido!" : "Tus estadísticas", 299, 22, DEPTH_UI, 0.55f, COLOR_GOLD, C2D_AlignCenter);
	if (s_welcome) {
		gfx_text_wrap("Elige tu Mii de la consola. Tus rivales son los demás Miis de la consola "
		              "(o Miis invitados, si los eliges).",
			220, 50, DEPTH_UI, 0.48f, COLOR_CREAM, 160);
		return;
	}
	for (int g = 0; g < GAME_COUNT; g++) {
		float y = 48 + g * 28;
		gfx_text(game_name((GameId)g), 220, y, DEPTH_UI, 0.48f, COLOR_CREAM, C2D_AlignLeft);
		if (p->played[g] == 0)
			gfx_text("sin jugar", 380, y + 12, DEPTH_UI, 0.45f, C2D_Color32(190, 170, 160, 255), C2D_AlignRight);
		else
			gfx_textf(380, y + 12, DEPTH_UI, 0.45f, COLOR_GOLD, C2D_AlignRight, "%u de %u ganadas", (unsigned)p->won[g],
				(unsigned)p->played[g]);
	}
}

static void profile_draw_bottom(void)
{
	const PlayerProfile* p = profile_get();
	gfx_background(BG_MENU_BOTTOM);
	button_draw(&s_btn_pick, true);

	/* Estado de las imágenes de los Miis. El detalle técnico de un fallo
	 * solo se ve manteniendo L y R (para poder diagnosticarlo). */
	const char* status = NULL;
	if (!p->avatar.official)
		status = "Todavía no has elegido tu Mii";
	else if (miiface_pending() > 0)
		status = "Preparando a tu Mii...";
	if (status)
		gfx_text(status, BOTTOM_W / 2, 74, 0, 0.5f, COLOR_CREAM, C2D_AlignCenter);
	if (has_error() && (hidKeysHeld() & (KEY_L | KEY_R)) == (KEY_L | KEY_R)) {
		gfx_text_wrap(miiface_last_error(), 20, 94, 0, 0.45f, C2D_Color32(255, 200, 190, 255), 280);
		button_draw(&s_btn_retry, false);
	}

	button_draw(&s_btn_rivals, false);
	button_draw(&s_btn_done, true);
}

const Scene scene_profile = { profile_enter, profile_update, profile_draw_top, profile_draw_bottom };
