#include "ui/mii.h"

#include <3ds.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "ui/gfx.h"
#include "ui/miiface.h"

/* ---------------------------------------------------------------------------
 * Dibujo
 * ------------------------------------------------------------------------- */

static u32 darker(uint32_t rgb, float f, u8 alpha)
{
	int r = (int)(((rgb >> 16) & 0xFF) * f), g = (int)(((rgb >> 8) & 0xFF) * f), b = (int)((rgb & 0xFF) * f);
	return C2D_Color32(r, g, b, alpha);
}

/* Curva (arco de parábola) hecha con segmentos: sonrisas, cejas, ojos cerrados */
static void curve(float x0, float x1, float y, float bend, float width, float depth, u32 color)
{
	const int steps = 6;
	float px = x0, py = y;
	for (int i = 1; i <= steps; i++) {
		float t = (float)i / steps;
		float x = x0 + (x1 - x0) * t;
		float yy = y + bend * 4.0f * t * (1.0f - t);
		gfx_line(px, py, x, yy, width, depth, color);
		px = x;
		py = yy;
	}
}

static void draw_back_hair(const Avatar* a, float cx, float cy, float r, float d, u32 hair)
{
	switch (a->hair_style) {
	case HAIR_LONG:
		gfx_rrect(cx - r * 1.12f, cy - r * 0.5f, r * 2.24f, r * 1.75f, r * 0.5f, d, hair);
		break;
	case HAIR_BOB:
		gfx_ellipse(cx, cy + r * 0.05f, r * 1.18f, r * 1.08f, d, hair);
		break;
	case HAIR_PONYTAIL:
		gfx_ellipse(cx + r * 1.0f, cy + r * 0.25f, r * 0.32f, r * 0.7f, d, hair);
		gfx_ellipse(cx + r * 0.75f, cy - r * 0.45f, r * 0.2f, r * 0.2f, d, darker(avatar_favorite_rgb(a->favorite), 1.0f, 255));
		break;
	case HAIR_BUN:
		gfx_ellipse(cx, cy - r * 1.08f, r * 0.42f, r * 0.36f, d, hair);
		break;
	case HAIR_CURLY:
		for (int i = 0; i < 7; i++) {
			float ang = (float)M_PI * (1.05f + i * 0.15f);
			gfx_ellipse(cx + cosf(ang) * r * 0.95f, cy + sinf(ang) * r * 0.95f, r * 0.34f, r * 0.34f, d, hair);
		}
		break;
	default:
		break;
	}
}

static void draw_front_hair(const Avatar* a, float cx, float cy, float r, float ry, float d, u32 hair, u32 skin)
{
	if (a->hair_style == HAIR_BALD) {
		gfx_ellipse(cx - r * 0.92f, cy - r * 0.1f, r * 0.16f, r * 0.32f, d, hair);
		gfx_ellipse(cx + r * 0.92f, cy - r * 0.1f, r * 0.16f, r * 0.32f, d, hair);
		return;
	}
	/* Casquete de pelo y, encima, la frente: queda el flequillo recortado */
	gfx_ellipse(cx, cy - ry * 0.3f, r * 1.04f, ry * 0.78f, d, hair);
	if (a->hair_style == HAIR_SPIKY) {
		for (int i = 0; i < 5; i++) {
			float x = cx + (i - 2) * r * 0.36f;
			gfx_triangle(x - r * 0.22f, cy - ry * 0.8f, x + r * 0.22f, cy - ry * 0.8f, x + (i - 2) * r * 0.08f,
				cy - ry * 1.35f, d, hair);
		}
	}
	float fy = a->hair_style == HAIR_SPIKY ? 0.2f : a->hair_style == HAIR_CURLY ? 0.24f : 0.17f;
	gfx_ellipse(cx + (a->hair_style == HAIR_SHORT ? r * 0.08f : 0), cy + ry * fy, r * 0.9f, ry * 0.86f, d, skin);
}

const char* mii_studio_expression(Expression e)
{
	switch (e) {
	case EXPR_HAPPY:
		return "smile";
	case EXPR_SAD:
		return "sorrow";
	case EXPR_SURPRISED:
		return "surprise_open_mouth";
	case EXPR_FRUSTRATED:
		return "frustrated";
	case EXPR_SMILE_OPEN:
		return "smile_open_mouth";
	case EXPR_ANGER:
		return "anger";
	case EXPR_BROWS_UP:
		return "surprise";
	case EXPR_WINK:
		return "wink_left";
	case EXPR_EYES_CLOSED:
		return "blink";
	default:
		return "normal";
	}
}

/* Cara oficial del Mii, si ya está disponible. */
static bool draw_official(const Avatar* a, float cx, float cy, float r, float d, Expression e, bool bust)
{
	if (!a->official)
		return false;
	bool online = profile_get()->online_miis;
	const C2D_Image* img = miiface_get(&a->mii, mii_studio_expression(e), online);
	if (!img && e != EXPR_NORMAL) {
		/* Mientras llega esa expresión, la cara normal (si ya está) */
		img = miiface_get(&a->mii, "normal", online);
	}
	if (!img)
		return false;
	if (bust) {
		u32 fav = gfx_rgb(avatar_favorite_rgb(a->favorite), 255);
		gfx_ellipse(cx, cy + r * 2.0f, r * 1.25f, r * 0.85f, d, fav);
		gfx_rect(cx - r * 0.28f, cy + r * 0.8f, r * 0.56f, r * 0.5f, d,
			gfx_rgb(avatar_skin_rgb(a->skin), 255));
	}
	/* La imagen de Mii Studio incluye el pelo: la cabeza ocupa unos 4/5 */
	float scale = r * 2.5f / img->subtex->width;
	C2D_DrawImageAtRotated(*img, cx + gfx_dx(d), cy, 0.5f, 0.0f, NULL, scale, scale);
	return true;
}

void mii_draw(const Avatar* a, float cx, float cy, float r, float d, Expression e, bool bust)
{
	if (draw_official(a, cx, cy, r, d, e, bust))
		return;
	uint32_t skin_rgb = avatar_skin_rgb(a->skin);
	u32 skin = gfx_rgb(skin_rgb, 255);
	u32 hair = gfx_rgb(avatar_hair_rgb(a->hair_color), 255);
	u32 fav = gfx_rgb(avatar_favorite_rgb(a->favorite), 255);
	u32 ink = C2D_Color32(30, 25, 25, 255);
	float ry = r * (a->long_face ? 1.15f : 1.04f);

	if (bust) {
		gfx_ellipse(cx, cy + ry + r * 0.95f, r * 1.25f, r * 0.85f, d, fav);
		/* Cuello y escote en pico */
		gfx_rect(cx - r * 0.28f, cy + ry * 0.6f, r * 0.56f, r * 0.55f, d, darker(skin_rgb, 0.9f, 255));
		gfx_triangle(cx - r * 0.3f, cy + ry + r * 0.12f, cx + r * 0.3f, cy + ry + r * 0.12f, cx, cy + ry + r * 0.55f, d,
			darker(skin_rgb, 0.9f, 255));
	}
	draw_back_hair(a, cx, cy, r, d, hair);

	/* Orejas y cara */
	gfx_ellipse(cx - r * 0.98f, cy + ry * 0.05f, r * 0.2f, r * 0.26f, d, darker(skin_rgb, 0.92f, 255));
	gfx_ellipse(cx + r * 0.98f, cy + ry * 0.05f, r * 0.2f, r * 0.26f, d, darker(skin_rgb, 0.92f, 255));
	gfx_ellipse(cx, cy, r, ry, d, skin);
	draw_front_hair(a, cx, cy, r, ry, d, hair, skin);

	/* Barba (por debajo de la boca) */
	if (a->facial == FACIAL_BEARD)
		gfx_ellipse(cx, cy + ry * 0.62f, r * 0.78f, ry * 0.42f, d, hair);

	/* Mejillas y nariz */
	gfx_ellipse(cx - r * 0.55f, cy + ry * 0.3f, r * 0.17f, r * 0.1f, d, C2D_Color32(255, 120, 120, 70));
	gfx_ellipse(cx + r * 0.55f, cy + ry * 0.3f, r * 0.17f, r * 0.1f, d, C2D_Color32(255, 120, 120, 70));
	gfx_ellipse(cx, cy + ry * 0.2f, r * 0.11f, r * 0.08f, d, darker(skin_rgb, 0.8f, 255));

	/* Gafas: montura y cristal claro, debajo de los ojos */
	float ex = r * 0.36f, ey = cy - ry * 0.02f;
	if (a->glasses) {
		for (int s = -1; s <= 1; s += 2) {
			gfx_ellipse(cx + s * ex, ey, r * 0.27f, r * 0.24f, d, ink);
			gfx_ellipse(cx + s * ex, ey, r * 0.21f, r * 0.18f, d, C2D_Color32(220, 240, 255, 255));
		}
		gfx_line(cx - ex + r * 0.25f, ey, cx + ex - r * 0.25f, ey, r * 0.06f, d, ink);
	}

	/* Cejas */
	float by = cy - ry * (e == EXPR_BROWS_UP || e == EXPR_SURPRISED ? 0.42f : 0.3f);
	float bw = r * (e == EXPR_BROWS_UP ? 0.11f : 0.07f) + 0.5f;
	for (int s = -1; s <= 1; s += 2) {
		float inner = e == EXPR_SAD ? -r * 0.08f : e == EXPR_ANGER ? r * 0.1f : 0.0f;
		gfx_line(cx + s * ex - r * 0.16f, by + (s < 0 ? 0 : inner), cx + s * ex + r * 0.16f, by + (s < 0 ? inner : 0), bw,
			d, hair);
	}

	/* Ojos */
	float eye_rx = r * 0.1f, eye_ry = r * (e == EXPR_SURPRISED || e == EXPR_BROWS_UP ? 0.18f : 0.15f);
	for (int s = -1; s <= 1; s += 2) {
		bool closed = e == EXPR_EYES_CLOSED || (e == EXPR_WINK && s > 0);
		float x = cx + s * ex;
		if (closed) {
			curve(x - r * 0.13f, x + r * 0.13f, ey, r * 0.06f, r * 0.06f + 0.5f, d, ink);
		} else if (e == EXPR_HAPPY) {
			curve(x - r * 0.13f, x + r * 0.13f, ey + r * 0.04f, -r * 0.1f, r * 0.07f + 0.5f, d, ink);
		} else {
			gfx_ellipse(x, ey, eye_rx, eye_ry, d, ink);
			gfx_ellipse(x + r * 0.03f, ey - r * 0.05f, r * 0.035f + 0.3f, r * 0.035f + 0.3f, d, COLOR_WHITE);
		}
	}

	/* Bigote */
	if (a->facial == FACIAL_MUSTACHE || a->facial == FACIAL_BEARD)
		gfx_ellipse(cx, cy + ry * 0.38f, r * 0.3f, r * 0.09f, d, hair);

	/* Boca */
	float my = cy + ry * 0.52f;
	float mw = r * 0.3f;
	u32 lip = C2D_Color32(170, 50, 60, 255);
	switch (e) {
	case EXPR_HAPPY:
		gfx_ellipse(cx, my, mw, r * 0.17f, d, lip);
		gfx_rect(cx - mw - 1, my - r * 0.18f, mw * 2 + 2, r * 0.18f, d, skin);
		gfx_ellipse(cx, my + r * 0.07f, mw * 0.5f, r * 0.06f, d, C2D_Color32(240, 120, 130, 255));
		break;
	case EXPR_SAD:
		curve(cx - mw * 0.8f, cx + mw * 0.8f, my + r * 0.05f, -r * 0.1f, r * 0.06f + 0.5f, d, lip);
		break;
	case EXPR_SURPRISED:
		gfx_ellipse(cx, my, r * 0.11f, r * 0.14f, d, lip);
		break;
	case EXPR_FRUSTRATED:
		/* labios apretados y algo torcidos */
		gfx_line(cx - mw * 0.7f, my + r * 0.03f, cx + mw * 0.7f, my - r * 0.03f, r * 0.1f + 0.5f, d, lip);
		break;
	case EXPR_SMILE_OPEN:
		gfx_ellipse(cx, my, mw, r * 0.17f, d, lip);
		gfx_rect(cx - mw - 1, my - r * 0.18f, mw * 2 + 2, r * 0.18f, d, skin);
		gfx_rect(cx - mw * 0.7f, my, mw * 1.4f, r * 0.05f, d, COLOR_WHITE);
		break;
	case EXPR_ANGER:
		gfx_line(cx - mw * 0.6f, my, cx + mw * 0.6f, my, r * 0.06f + 0.5f, d, lip);
		break;
	default:
		curve(cx - mw * 0.8f, cx + mw * 0.8f, my, r * 0.08f, r * 0.06f + 0.5f, d, lip);
		break;
	}
}

void mood_set(Mood* m, Expression e, float seconds)
{
	m->expr = e;
	m->timer = seconds;
}

void mood_update(Mood* m, float dt)
{
	if (m->timer > 0.0f) {
		m->timer -= dt;
		if (m->timer <= 0.0f)
			m->expr = EXPR_NORMAL;
	}
	m->blink += dt;
	if (m->blink > 3.4f)
		m->blink -= 3.4f;
}

Expression mood_expr(const Mood* m)
{
	if (m->expr == EXPR_NORMAL && m->blink > 3.25f)
		return EXPR_EYES_CLOSED;
	return m->expr;
}

/* ---------------------------------------------------------------------------
 * Perfil
 * ------------------------------------------------------------------------- */

#define PROFILE_DIR  "sdmc:/3ds/JuegosDeCartas"
#define PROFILE_PATH PROFILE_DIR "/perfil.dat"

static PlayerProfile s_profile;
static bool s_loaded;

PlayerProfile* profile_get(void)
{
	if (!s_loaded) {
		if (!profile_load(&s_profile, PROFILE_PATH))
			profile_init(&s_profile);
		s_loaded = true;
	}
	return &s_profile;
}

void profile_store(void)
{
	mkdir("sdmc:/3ds", 0777);
	mkdir(PROFILE_DIR, 0777);
	profile_save(profile_get(), PROFILE_PATH);
}

void profile_record_game(GameId game, bool won)
{
	profile_record(profile_get(), game, won);
	profile_store();
}

/* Copia todas las piezas de un Mii de la consola. */
static void parts_from_console(MiiParts* m, const MiiData* mii)
{
	memset(m, 0, sizeof *m);
	m->gender = mii->mii_details.sex;
	m->favorite_color = mii->mii_details.shirt_color;
	m->height = mii->height;
	m->build = mii->width;
	m->face_type = mii->face_style.shape;
	m->face_color = mii->face_style.skinColor;
	m->face_wrinkles = mii->face_details.wrinkles;
	m->face_makeup = mii->face_details.makeup;
	m->hair_type = mii->hair_style;
	m->hair_color = mii->hair_details.color;
	m->hair_flip = mii->hair_details.flip;
	m->eye_type = mii->eye_details.style;
	m->eye_color = mii->eye_details.color;
	m->eye_scale = mii->eye_details.scale;
	m->eye_yscale = mii->eye_details.yscale;
	m->eye_rotation = mii->eye_details.rotation;
	m->eye_xspacing = mii->eye_details.xspacing;
	m->eye_y = mii->eye_details.yposition;
	m->brow_type = mii->eyebrow_details.style;
	m->brow_color = mii->eyebrow_details.color;
	m->brow_scale = mii->eyebrow_details.scale;
	m->brow_yscale = mii->eyebrow_details.yscale;
	m->brow_rotation = mii->eyebrow_details.rotation;
	m->brow_xspacing = mii->eyebrow_details.xspacing;
	m->brow_y = mii->eyebrow_details.yposition;
	m->nose_type = mii->nose_details.style;
	m->nose_scale = mii->nose_details.scale;
	m->nose_y = mii->nose_details.yposition;
	m->mouth_type = mii->mouth_details.style;
	m->mouth_color = mii->mouth_details.color;
	m->mouth_scale = mii->mouth_details.scale;
	m->mouth_yscale = mii->mouth_details.yscale;
	m->mouth_y = mii->mustache_details.mouth_yposition;
	m->mustache_type = mii->mustache_details.mustache_style;
	m->beard_type = mii->beard_details.style;
	m->facial_hair_color = mii->beard_details.color;
	m->mustache_scale = mii->beard_details.scale;
	m->mustache_y = mii->beard_details.ypos;
	m->glasses_type = mii->glasses_details.style;
	m->glasses_color = mii->glasses_details.color;
	m->glasses_scale = mii->glasses_details.scale;
	m->glasses_y = mii->glasses_details.ypos;
	m->mole_enabled = mii->mole_details.enable;
	m->mole_scale = mii->mole_details.scale;
	m->mole_x = mii->mole_details.xpos;
	m->mole_y = mii->mole_details.ypos;
	mii_parts_sanitize(m);
}

bool profile_pick_console_mii(void)
{
	MiiSelectorConf conf;
	MiiSelectorReturn ret;
	miiSelectorInit(&conf);
	miiSelectorSetTitle(&conf, "Elige tu Mii");
	miiSelectorSetOptions(&conf, MIISELECTOR_CANCEL);
	miiSelectorLaunch(&conf, &ret);
	if (ret.no_mii_selected || !miiSelectorChecksumIsValid(&ret))
		return false;
	char name[AVATAR_NAME_MAX];
	miiSelectorReturnGetName(&ret, name, sizeof name);
	PlayerProfile* p = profile_get();
	MiiParts parts;
	parts_from_console(&parts, &ret.mii);
	avatar_from_mii(&p->avatar, &parts, name);
	p->from_console = 1;
	p->created = 1;
	profile_store();
	return true;
}

bool profile_edit_name(void)
{
	PlayerProfile* p = profile_get();
	SwkbdState kb;
	char buf[AVATAR_NAME_MAX];
	swkbdInit(&kb, SWKBD_TYPE_NORMAL, 2, 10);
	swkbdSetHintText(&kb, "Tu nombre");
	swkbdSetInitialText(&kb, p->avatar.name);
	swkbdSetValidation(&kb, SWKBD_NOTEMPTY_NOTBLANK, 0, 0);
	if (swkbdInputText(&kb, buf, sizeof buf) != SWKBD_BUTTON_CONFIRM)
		return false;
	snprintf(p->avatar.name, sizeof p->avatar.name, "%s", buf);
	avatar_sanitize(&p->avatar);
	profile_store();
	return true;
}
