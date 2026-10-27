/* Perfil del jugador: su Mii (de la consola o hecho aquí) y sus estadísticas. */
#include <math.h>
#include <stdio.h>

#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/mii.h"
#include "ui/miiface.h"
#include "ui/widgets.h"

typedef enum {
	ROW_CONSOLE,
	ROW_NAME,
	ROW_ONLINE,
	ROW_SEX,
	ROW_SKIN,
	ROW_HAIR,
	ROW_HAIR_COLOR,
	ROW_FAVORITE,
	ROW_GLASSES,
	ROW_FACIAL,
	ROW_FACE,
	ROW_COUNT
} Row;

#define ROW_X 14
#define ROW_Y 8
#define ROW_W 292
#define ROW_H 16
#define ROW_STEP 17

static int s_row;
static Mood s_mood;
static Button s_btn_done, s_btn_rivals;
static bool s_welcome;
static float s_saved_flash;

static void profile_enter(void)
{
	s_welcome = !profile_get()->created;
	s_row = s_welcome ? ROW_NAME : ROW_CONSOLE;
	mood_set(&s_mood, EXPR_HAPPY, 1.5f);
	button_set(&s_btn_rivals, 12, 205, 140, 28, GLYPH_X " Tus rivales", C2D_Color32(60, 90, 160, 255), KEY_X);
	button_set(&s_btn_done, 160, 205, 148, 28, s_welcome ? GLYPH_A " ¡A jugar!" : GLYPH_B " Listo",
		C2D_Color32(40, 140, 60, 255), s_welcome ? 0 : KEY_B);
}

static void change(int row, int delta)
{
	PlayerProfile* p = profile_get();
	Avatar* a = &p->avatar;
	if (row == ROW_ONLINE) {
		p->online_miis = !p->online_miis;
		if (p->online_miis)
			miiface_retry();
		audio_play(SFX_SELECT);
		return;
	}
	/* Retocar el dibujo deja de usar el Mii de verdad */
	a->official = 0;
	switch (row) {
	case ROW_SEX:
		a->female = !a->female;
		break;
	case ROW_SKIN:
		a->skin = (a->skin + AVATAR_SKINS + delta) % AVATAR_SKINS;
		break;
	case ROW_HAIR:
		a->hair_style = (a->hair_style + HAIR_STYLES + delta) % HAIR_STYLES;
		break;
	case ROW_HAIR_COLOR:
		a->hair_color = (a->hair_color + AVATAR_HAIR_COLORS + delta) % AVATAR_HAIR_COLORS;
		break;
	case ROW_FAVORITE:
		a->favorite = (a->favorite + AVATAR_FAVORITES + delta) % AVATAR_FAVORITES;
		break;
	case ROW_GLASSES:
		a->glasses = !a->glasses;
		break;
	case ROW_FACIAL:
		a->facial = (a->facial + FACIAL_STYLES + delta) % FACIAL_STYLES;
		break;
	case ROW_FACE:
		a->long_face = !a->long_face;
		break;
	default:
		return;
	}
	profile_get()->from_console = 0;
	audio_play(SFX_SELECT);
	mood_set(&s_mood, EXPR_SURPRISED, 0.4f);
}

static void activate(int row)
{
	if (row == ROW_CONSOLE) {
		if (profile_pick_console_mii()) {
			audio_play(SFX_WIN);
			mood_set(&s_mood, EXPR_HAPPY, 2.0f);
		}
	} else if (row == ROW_NAME) {
		if (profile_edit_name())
			mood_set(&s_mood, EXPR_HAPPY, 1.5f);
	}
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
	if (s_saved_flash > 0.0f)
		s_saved_flash -= dt;

	if (in->repeat & KEY_DOWN) {
		s_row = (s_row + 1) % ROW_COUNT;
		audio_play(SFX_SELECT);
	}
	if (in->repeat & KEY_UP) {
		s_row = (s_row + ROW_COUNT - 1) % ROW_COUNT;
		audio_play(SFX_SELECT);
	}
	if (in->repeat & KEY_LEFT)
		change(s_row, -1);
	if (in->repeat & KEY_RIGHT)
		change(s_row, 1);
	if (in->down & KEY_Y)
		miiface_retry();
	if (in->down & KEY_A) {
		if (s_row <= ROW_NAME)
			activate(s_row);
		else if (s_welcome)
			return finish();
		else
			change(s_row, 1);
	}
	if (!s_welcome && (in->down & KEY_START))
		return finish();
	if (button_update(&s_btn_done, in))
		return finish();
	if (button_update(&s_btn_rivals, in)) {
		finish();
		return SCENE_RIVALS;
	}

	for (int r = 0; r < ROW_COUNT; r++) {
		float y = ROW_Y + r * ROW_STEP;
		if (!input_touch_in(in, ROW_X, y, ROW_W, ROW_H))
			continue;
		s_row = r;
		if (r <= ROW_NAME)
			activate(r);
		else
			change(r, in->tx < ROW_X + ROW_W / 2 + 40 ? -1 : 1);
	}
	return SCENE_PROFILE;
}

static void profile_draw_top(void)
{
	const PlayerProfile* p = profile_get();
	gfx_background(BG_TITLE_TOP);

	/* Pedestal y Mii */
	float bob = sinf(gfx_time() * 2.0f) * 2.0f;
	gfx_ellipse(110, 214, 70, 12, DEPTH_TABLE, C2D_Color32(0, 0, 0, 90));
	mii_draw(&p->avatar, 110, 92 + bob, 44, DEPTH_BANNER, mood_expr(&s_mood), true);
	gfx_panel(40, 192, 140, 30, DEPTH_UI, C2D_Color32(110, 20, 25, 230), COLOR_GOLD);
	gfx_text(p->avatar.name, 110, 197, DEPTH_UI, 0.6f, COLOR_CREAM, C2D_AlignCenter);
	const char* status = NULL;
	if (p->avatar.official) {
		if (miiface_pending() > 0)
			status = "Descargando Mii oficial...";
		else if (!miiface_get(&p->avatar.mii, "normal", false))
			status = p->online_miis ? "Sin conexión: Mii dibujado" : "Mii oficial desactivado";
		else
			status = "Mii oficial";
	} else {
		status = "Mii dibujado";
	}
	gfx_text(status, 110, 12, DEPTH_UI, 0.42f, COLOR_GOLD, C2D_AlignCenter);

	/* Estadísticas */
	gfx_panel(210, 16, 178, 208, DEPTH_UI, C2D_Color32(40, 8, 12, 210), C2D_Color32(230, 170, 50, 255));
	gfx_text(s_welcome ? "¡Bienvenido!" : "Tus estadísticas", 299, 22, DEPTH_UI, 0.55f, COLOR_GOLD, C2D_AlignCenter);
	if (s_welcome) {
		gfx_text_wrap("Elige tu Mii de la consola para jugar con tu Mii de verdad (su cara oficial se "
		              "descarga una vez de Mii Studio de Nintendo), o diséñalo aquí abajo. "
		              "Tus rivales también son Miis.",
			220, 50, DEPTH_UI, 0.45f, COLOR_CREAM, 160);
		return;
	}
	for (int g = 0; g < GAME_COUNT; g++) {
		float y = 50 + g * 28;
		gfx_text(game_name((GameId)g), 222, y, DEPTH_UI, 0.45f, COLOR_CREAM, C2D_AlignLeft);
		if (p->played[g] == 0)
			gfx_text("sin jugar", 378, y + 11, DEPTH_UI, 0.38f, C2D_Color32(180, 160, 150, 255), C2D_AlignRight);
		else
			gfx_textf(378, y + 11, DEPTH_UI, 0.38f, COLOR_GOLD, C2D_AlignRight, "%u de %u %s ganadas", (unsigned)p->won[g],
				(unsigned)p->played[g], game_unit((GameId)g));
	}
}

static void swatch(float x, float y, uint32_t rgb)
{
	gfx_rrect(x - 1, y - 1, 26, 13, 3, 0, COLOR_BLACK);
	gfx_rrect(x, y, 24, 11, 3, 0, gfx_rgb(rgb, 255));
}

static void profile_draw_bottom(void)
{
	const Avatar* a = &profile_get()->avatar;
	gfx_background(BG_MENU_BOTTOM);

	static const char* const LABELS[ROW_COUNT] = { "Usar un Mii de la consola", "Nombre", "Miis oficiales (internet)",
		"Sexo", "Piel", "Peinado", "Color de pelo", "Color favorito", "Gafas", "Barba", "Cara" };
	static const char* const FACIAL[FACIAL_STYLES] = { "Ninguna", "Barba", "Bigote" };

	for (int r = 0; r < ROW_COUNT; r++) {
		float y = ROW_Y + r * ROW_STEP;
		bool sel = r == s_row;
		u32 fill = sel ? C2D_Color32(255, 236, 170, 255) : C2D_Color32(250, 244, 228, 235);
		gfx_rrect(ROW_X, y, ROW_W, ROW_H, 5, 0, fill);
		if (sel)
			gfx_rrect(ROW_X, y + ROW_H - 3, ROW_W, 3, 1, 0, COLOR_GOLD);
		u32 ink = C2D_Color32(90, 15, 20, 255);
		if (r == ROW_CONSOLE) {
			gfx_text(GLYPH_A " Elegir mi Mii (de la consola o invitado)", ROW_X + ROW_W / 2, y + 1, 0, 0.43f, ink, C2D_AlignCenter);
			continue;
		}
		if (r >= ROW_SEX && a->official)
			ink = C2D_Color32(150, 130, 120, 255); /* retocar pasa al Mii dibujado */
		gfx_text(LABELS[r], ROW_X + 8, y + 1, 0, 0.43f, ink, C2D_AlignLeft);
		float vx = ROW_X + ROW_W - 70;
		char value[40] = "";
		switch (r) {
		case ROW_NAME:
			snprintf(value, sizeof value, "%s", a->name);
			break;
		case ROW_ONLINE:
			snprintf(value, sizeof value, "%s", profile_get()->online_miis ? "Sí" : "No");
			break;
		case ROW_SEX:
			snprintf(value, sizeof value, "%s", a->female ? "Mujer" : "Hombre");
			break;
		case ROW_SKIN:
			swatch(vx - 12, y + 2, avatar_skin_rgb(a->skin));
			break;
		case ROW_HAIR:
			snprintf(value, sizeof value, "%s", avatar_hair_style_name(a->hair_style));
			break;
		case ROW_HAIR_COLOR:
			swatch(vx - 12, y + 2, avatar_hair_rgb(a->hair_color));
			break;
		case ROW_FAVORITE:
			swatch(vx - 50, y + 2, avatar_favorite_rgb(a->favorite));
			snprintf(value, sizeof value, "%s", avatar_favorite_name(a->favorite));
			vx += 10;
			break;
		case ROW_GLASSES:
			snprintf(value, sizeof value, "%s", a->glasses ? "Sí" : "No");
			break;
		case ROW_FACIAL:
			snprintf(value, sizeof value, "%s", FACIAL[a->facial]);
			break;
		case ROW_FACE:
			snprintf(value, sizeof value, "%s", a->long_face ? "Alargada" : "Redonda");
			break;
		}
		if (value[0])
			gfx_text(value, vx, y + 1, 0, 0.43f, ink, C2D_AlignCenter);
		if (r != ROW_NAME) {
			/* Flechas para cambiar el valor */
			float ax = ROW_X + ROW_W - 136, bx = ROW_X + ROW_W - 8, cy = y + ROW_H / 2;
			gfx_triangle(ax + 8, cy - 5, ax + 8, cy + 5, ax, cy, 0, ink);
			gfx_triangle(bx - 8, cy - 5, bx - 8, cy + 5, bx, cy, 0, ink);
		}
	}
	button_draw(&s_btn_rivals, false);
	button_draw(&s_btn_done, true);
}

const Scene scene_profile = { profile_enter, profile_update, profile_draw_top, profile_draw_bottom };
