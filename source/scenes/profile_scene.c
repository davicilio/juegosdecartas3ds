/* Perfil del jugador: su personaje (basado en un Mii de la consola) y sus
 * estadísticas. */
#include <math.h>
#include <stdio.h>

#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/mii.h"
#include "ui/widgets.h"

static Mood s_mood;
static Button s_btn_pick, s_btn_rivals, s_btn_done;
static bool s_welcome;

static void profile_enter(void)
{
	s_welcome = !profile_get()->created;
	mood_set(&s_mood, EXPR_HAPPY, 1.5f);
	button_set(&s_btn_pick, 40, 20, 240, 40, GLYPH_A " Elegir mi Mii", C2D_Color32(40, 140, 60, 255), KEY_A);
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

static SceneId profile_update(const Input* in, float dt)
{
	mood_update(&s_mood, dt);

	if (button_update(&s_btn_pick, in)) {
		if (profile_pick_console_mii()) {
			audio_play(SFX_WIN);
			mood_set(&s_mood, EXPR_HAPPY, 2.0f);
		}
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

	/* Tu personaje de cuerpo entero */
	gfx_ellipse(110, 196, 56, 10, DEPTH_TABLE, C2D_Color32(0, 0, 0, 90));
	mii_draw_body(&p->avatar, 110, 198, 182, DEPTH_BANNER, mood_expr(&s_mood));
	gfx_panel(30, 200, 160, 28, DEPTH_UI, C2D_Color32(110, 20, 25, 230), COLOR_GOLD);
	gfx_text(p->avatar.name, 110, 204, DEPTH_UI, 0.55f, COLOR_CREAM, C2D_AlignCenter);

	/* Estadísticas */
	gfx_panel(210, 16, 178, 208, DEPTH_UI, C2D_Color32(40, 8, 12, 210), C2D_Color32(230, 170, 50, 255));
	gfx_text(s_welcome ? "¡Bienvenido!" : "Tus estadísticas", 299, 22, DEPTH_UI, 0.55f, COLOR_GOLD, C2D_AlignCenter);
	if (s_welcome) {
		gfx_text_wrap("Elige un Mii de la consola y tu personaje se parecerá a él: nombre, piel, "
		              "pelo, color favorito... Tus rivales se basan en los demás Miis.",
			220, 50, DEPTH_UI, 0.48f, COLOR_CREAM, 160);
		return;
	}
	/* Una línea por juego: nombre a la izquierda y resultado a la derecha */
	for (int g = 0; g < GAME_COUNT; g++) {
		float y = 42 + g * 18;
		gfx_text(game_name((GameId)g), 218, y, DEPTH_UI, 0.45f, COLOR_CREAM, C2D_AlignLeft);
		if (p->played[g] == 0)
			gfx_text("-", 380, y, DEPTH_UI, 0.45f, C2D_Color32(190, 170, 160, 255), C2D_AlignRight);
		else
			gfx_textf(380, y, DEPTH_UI, 0.45f, COLOR_GOLD, C2D_AlignRight, "%u/%u", (unsigned)p->won[g],
				(unsigned)p->played[g]);
	}
	gfx_text("ganadas / jugadas", 299, 42 + GAME_COUNT * 18, DEPTH_UI, 0.45f, C2D_Color32(190, 170, 160, 255),
		C2D_AlignCenter);
}

static void profile_draw_bottom(void)
{
	const PlayerProfile* p = profile_get();
	gfx_background(BG_MENU_BOTTOM);
	button_draw(&s_btn_pick, true);

	gfx_text(p->avatar.official ? "Basado en tu Mii de la consola" : "Todavía no has elegido tu Mii", BOTTOM_W / 2, 74, 0,
		0.5f, COLOR_CREAM, C2D_AlignCenter);
	gfx_text_wrap("Tu personaje copia el nombre, la piel, el pelo, el color favorito, las gafas y la barba del Mii.",
		24, 100, 0, 0.45f, C2D_Color32(230, 215, 190, 255), 272);

	button_draw(&s_btn_rivals, false);
	button_draw(&s_btn_done, true);
}

const Scene scene_profile = { profile_enter, profile_update, profile_draw_top, profile_draw_bottom };
