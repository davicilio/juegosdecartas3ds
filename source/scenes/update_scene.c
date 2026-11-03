/* Actualizar el juego: busca la versión publicada en GitHub y la instala. */
#include <math.h>
#include <stdio.h>

#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/updater.h"
#include "ui/widgets.h"

static Button s_btn_main, s_btn_back;
static UpdState s_prev;

static void up_enter(void)
{
	/* Se vuelve a mirar siempre al entrar: lo de al arrancar puede haberse
	 * quedado viejo (por ejemplo, si se publicó una versión después) */
	UpdState st = updater_state();
	if (st != UPD_CHECKING && st != UPD_DOWNLOADING && st != UPD_DONE)
		updater_check();
	s_prev = updater_state();
	button_set(&s_btn_back, 164, 205, 144, 28, GLYPH_B " Volver", C2D_Color32(120, 120, 120, 255), KEY_B);
}

static SceneId up_update(const Input* in, float dt)
{
	(void)dt;
	UpdState st = updater_state();
	if (st != s_prev) {
		if (st == UPD_AVAILABLE)
			audio_play(SFX_CONFIRM);
		else if (st == UPD_DONE)
			audio_play(SFX_WIN);
		else if (st == UPD_ERROR)
			audio_play(SFX_ERROR);
		s_prev = st;
	}
	const char* label = st == UPD_AVAILABLE ? GLYPH_A " Instalar" : st == UPD_DONE ? GLYPH_A " Salir del juego"
		: GLYPH_A " Buscar otra vez";
	button_set(&s_btn_main, 12, 205, 144, 28, label, C2D_Color32(40, 140, 60, 255), KEY_A);
	s_btn_main.enabled = st != UPD_CHECKING && st != UPD_DOWNLOADING;
	s_btn_back.enabled = st != UPD_DOWNLOADING;
	if (button_update(&s_btn_main, in)) {
		if (st == UPD_AVAILABLE)
			updater_install();
		else if (st == UPD_DONE)
			return SCENE_QUIT;
		else
			updater_check();
	} else if (button_update(&s_btn_back, in)) {
		return SCENE_MENU;
	}
	return SCENE_UPDATE;
}

static void up_draw_top(void)
{
	gfx_background(BG_TITLE_TOP);
	gfx_sprite(SPR_LOGO, 200, 50, 1.0f, DEPTH_BANNER);
	gfx_panel(60, 110, 280, 100, DEPTH_UI, C2D_Color32(40, 8, 12, 210), C2D_Color32(230, 170, 50, 255));
	gfx_text("Actualizar el juego", 200, 116, DEPTH_UI, 0.6f, COLOR_GOLD, C2D_AlignCenter);
	if (updater_build())
		gfx_textf(200, 146, DEPTH_UI, 0.5f, COLOR_CREAM, C2D_AlignCenter, "Tienes la versión %d", updater_build());
	else
		gfx_text("Tienes una versión compilada a mano", 200, 146, DEPTH_UI, 0.5f, COLOR_CREAM, C2D_AlignCenter);
	if (updater_remote_build())
		gfx_textf(200, 168, DEPTH_UI, 0.5f, COLOR_CREAM, C2D_AlignCenter, "La última publicada es la %d",
			updater_remote_build());
	gfx_text(updater_is_cia() ? "Instalado como aplicación (.cia)" : "Abierto desde el Homebrew Launcher (.3dsx)", 200,
		190, DEPTH_UI, 0.42f, C2D_Color32(200, 180, 170, 255), C2D_AlignCenter);
}

static void up_draw_bottom(void)
{
	gfx_background(BG_MENU_BOTTOM);
	UpdState st = updater_state();
	const char* msg = "";
	switch (st) {
	case UPD_CHECKING:
		msg = "Buscando versiones nuevas...";
		break;
	case UPD_UP_TO_DATE: {
		static char buf[80];
		snprintf(buf, sizeof buf, "Ya tienes la última versión (la publicada es la %d). ¡A jugar!", updater_remote_build());
		msg = buf;
		break;
	}
	case UPD_AVAILABLE:
		msg = "Hay una versión nueva. Tu perfil, tus fichas y tus partidas guardadas se conservan.";
		break;
	case UPD_DOWNLOADING:
		msg = updater_is_cia() ? "Descargando e instalando... no apagues la consola" : "Descargando... no apagues la consola";
		break;
	case UPD_DONE:
		msg = "¡Listo! Sal del juego y vuelve a abrirlo para usar la versión nueva.";
		break;
	case UPD_ERROR:
		msg = updater_error()[0] ? updater_error() : "No se ha podido conectar";
		break;
	default:
		break;
	}
	gfx_text_wrap(msg, 20, 30, 0, 0.5f, st == UPD_ERROR ? C2D_Color32(255, 170, 160, 255) : COLOR_CREAM, 280);
	if (st == UPD_ERROR)
		gfx_text_wrap("Comprueba que la conexión a internet de la consola funciona. El repositorio del juego tiene que "
		              "ser público para poder descargarlo.",
			20, 96, 0, 0.42f, C2D_Color32(230, 215, 190, 255), 280);
	if (st == UPD_CHECKING || st == UPD_DOWNLOADING) {
		float p = updater_progress();
		gfx_rrect(30, 150, 260, 18, 6, 0, C2D_Color32(30, 20, 20, 255));
		if (st == UPD_DOWNLOADING && p >= 0) {
			gfx_rrect(32, 152, 256 * fminf(1.0f, p), 14, 5, 0, C2D_Color32(60, 170, 80, 255));
			gfx_textf(160, 172, 0, 0.45f, COLOR_CREAM, C2D_AlignCenter, "%d %%", (int)(p * 100));
		} else {
			/* Sin tamaño conocido: una barrita que va y viene */
			float x = 32 + (0.5f + 0.5f * sinf(gfx_time() * 3.0f)) * 200;
			gfx_rrect(x, 152, 56, 14, 5, 0, C2D_Color32(60, 170, 80, 255));
		}
	}
	button_draw(&s_btn_main, s_btn_main.enabled);
	button_draw(&s_btn_back, false);
}

const Scene scene_update = { up_enter, up_update, up_draw_top, up_draw_bottom };
