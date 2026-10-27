#include "ui/mii.h"

#include <3ds.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "ui/chars_layout.h"
#include "ui/gfx.h"

/* ---------------------------------------------------------------------------
 * Dibujo de los personajes
 *
 * Cada personaje se monta con piezas del atlas gfx/chars.t3s (generadas por
 * tools/gen_characters.py). Las piezas en gris se tiñen multiplicando por el
 * color de piel, pelo o color favorito del Mii en el que se basa.
 * ------------------------------------------------------------------------- */

typedef struct {
	u8 eye_left, eye_right; /* CHR_EYE_* (CHR_EYE_OPEN se cambia por pestañas en las chicas) */
	float brow_dy;          /* subir (negativo) o bajar las cejas, en píxeles del atlas */
	float brow_angle;       /* giro de la ceja izquierda; la derecha gira al revés */
	u8 mouth;
} Face;

static const Face FACES[EXPR_COUNT] = {
	[EXPR_NORMAL] = { CHR_EYE_OPEN, CHR_EYE_OPEN, 0.0f, 0.0f, CHR_MOUTH_SMILE },
	[EXPR_HAPPY] = { CHR_EYE_HAPPY, CHR_EYE_HAPPY, -3.0f, 0.0f, CHR_MOUTH_HAPPY },
	[EXPR_SAD] = { CHR_EYE_OPEN, CHR_EYE_OPEN, 0.0f, -0.28f, CHR_MOUTH_SAD },
	[EXPR_SURPRISED] = { CHR_EYE_WIDE, CHR_EYE_WIDE, -6.0f, 0.0f, CHR_MOUTH_O },
	[EXPR_BITE_LIP] = { CHR_EYE_OPEN, CHR_EYE_OPEN, 0.0f, 0.0f, CHR_MOUTH_BITE },
	[EXPR_TONGUE] = { CHR_EYE_OPEN, CHR_EYE_OPEN, 0.0f, 0.0f, CHR_MOUTH_TONGUE },
	[EXPR_TWIST] = { CHR_EYE_OPEN, CHR_EYE_OPEN, 1.0f, 0.12f, CHR_MOUTH_TWIST },
	[EXPR_BROWS_UP] = { CHR_EYE_OPEN, CHR_EYE_OPEN, -9.0f, 0.0f, CHR_MOUTH_SMILE },
	[EXPR_WINK] = { CHR_EYE_HAPPY, CHR_EYE_OPEN, 0.0f, 0.0f, CHR_MOUTH_SMILE },
	[EXPR_EYES_CLOSED] = { CHR_EYE_CLOSED, CHR_EYE_CLOSED, 0.0f, 0.0f, CHR_MOUTH_SMILE },
};

static const u8 HAIR_FRONT[HAIR_STYLES] = {
	CHR_HAIR_SHORT_FRONT, CHR_HAIR_LONG_FRONT, CHR_HAIR_BUN_FRONT, CHR_HAIR_SPIKY_FRONT,
	CHR_HAIR_BALD_FRONT, CHR_HAIR_BOB_FRONT, CHR_HAIR_PONYTAIL_FRONT, CHR_HAIR_CURLY_FRONT,
};
static const u8 HAIR_BACK[HAIR_STYLES] = {
	CHR_HAIR_SHORT_BACK, CHR_HAIR_LONG_BACK, CHR_HAIR_BUN_BACK, CHR_HAIR_SPIKY_BACK,
	CHR_HAIR_BALD_BACK, CHR_HAIR_BOB_BACK, CHR_HAIR_PONYTAIL_BACK, CHR_HAIR_CURLY_BACK,
};

/* Versión del lado derecho de la cara de cada pieza que la tiene. */
static int right_side(int id)
{
	switch (id) {
	case CHR_BLUSH:
		return CHR_BLUSH_R;
	case CHR_EYE_OPEN:
		return CHR_EYE_OPEN_R;
	case CHR_EYE_LASHES:
		return CHR_EYE_LASHES_R;
	case CHR_EYE_WIDE:
		return CHR_EYE_WIDE_R;
	case CHR_EYE_CLOSED:
		return CHR_EYE_CLOSED_R;
	case CHR_EYE_HAPPY:
		return CHR_EYE_HAPPY_R;
	case CHR_BROW:
		return CHR_BROW_R;
	default:
		return id;
	}
}

/* Pieza id con el centro de la cabeza en (hx, hy) y escala s. Con right se
 * usa su versión del lado derecho de la cara. */
static void piece(int id, float hx, float hy, float s, const C2D_ImageTint* tint, bool right, float angle, float dy)
{
	if (right)
		id = right_side(id);
	C2D_Image img = gfx_char_image(id);
	C2D_DrawImageAtRotated(img, hx + CHR_OFFSET[id][0] * s, hy + (CHR_OFFSET[id][1] + dy) * s, 0.5f, angle, tint, s, s);
}

static void tint_rgb(C2D_ImageTint* t, uint32_t rgb)
{
	C2D_PlainImageTint(t, gfx_rgb(rgb, 255), 1.0f);
}

/* El pelo muy oscuro se aclara un poco para que se vean los brillos. */
static uint32_t hair_rgb(int color)
{
	uint32_t c = avatar_hair_rgb(color), out = 0;
	for (int sh = 0; sh <= 16; sh += 8) {
		uint32_t v = (c >> sh) & 0xFF;
		v += (0xFF - v) * 30 / 255;
		out |= v << sh;
	}
	return out;
}

typedef enum { PART_HEAD, PART_BUST, PART_BODY } Parts;

static void draw_character(const Avatar* a, float hx, float hy, float s, float d, Expression e, Parts parts)
{
	const Face* f = &FACES[(unsigned)e < EXPR_COUNT ? e : EXPR_NORMAL];
	C2D_ImageTint skin, hair, shirt;
	tint_rgb(&skin, avatar_skin_rgb(a->skin));
	tint_rgb(&hair, hair_rgb(a->hair_color));
	tint_rgb(&shirt, avatar_favorite_rgb(a->favorite));
	hx += gfx_dx(d);

	C2D_SetTintMode(C2D_TintMult);
	if (parts == PART_BODY) {
		if (a->female)
			piece(CHR_SHINS, hx, hy, s, &skin, false, 0.0f, 0.0f);
		piece(a->female ? CHR_SKIRT : CHR_LEGS, hx, hy, s, NULL, false, 0.0f, 0.0f);
	}
	if (parts != PART_HEAD) {
		piece(CHR_TORSO, hx, hy, s, &shirt, false, 0.0f, 0.0f);
		piece(CHR_ARMS, hx, hy, s, &skin, false, 0.0f, 0.0f);
	}
	piece(HAIR_BACK[a->hair_style % HAIR_STYLES], hx, hy, s, &hair, false, 0.0f, 0.0f);
	piece(CHR_HEAD, hx, hy, s, &skin, false, 0.0f, 0.0f);
	piece(CHR_BLUSH, hx, hy, s, NULL, false, 0.0f, 0.0f);
	piece(CHR_BLUSH, hx, hy, s, NULL, true, 0.0f, 0.0f);
	piece(CHR_NOSE, hx, hy, s, &skin, false, 0.0f, 0.0f);
	if (a->facial == FACIAL_BEARD)
		piece(CHR_BEARD, hx, hy, s, &hair, false, 0.0f, 0.0f);

	int open = a->female ? CHR_EYE_LASHES : CHR_EYE_OPEN;
	piece(f->eye_left == CHR_EYE_OPEN ? open : f->eye_left, hx, hy, s, NULL, false, 0.0f, 0.0f);
	piece(f->eye_right == CHR_EYE_OPEN ? open : f->eye_right, hx, hy, s, NULL, true, 0.0f, 0.0f);
	piece(CHR_BROW, hx, hy, s, &hair, false, f->brow_angle, f->brow_dy);
	piece(CHR_BROW, hx, hy, s, &hair, true, -f->brow_angle, f->brow_dy);
	piece(f->mouth, hx, hy, s, NULL, false, 0.0f, 0.0f);
	if (a->facial == FACIAL_MUSTACHE)
		piece(CHR_MUSTACHE, hx, hy, s, &hair, false, 0.0f, 0.0f);
	piece(HAIR_FRONT[a->hair_style % HAIR_STYLES], hx, hy, s, &hair, false, 0.0f, 0.0f);
	if (a->glasses)
		piece(CHR_GLASSES, hx, hy, s, NULL, false, 0.0f, 0.0f);
	C2D_SetTintMode(C2D_TintSolid);
}

void mii_draw(const Avatar* a, float cx, float cy, float r, float d, Expression e, bool bust)
{
	draw_character(a, cx, cy, r * 1.05f / CHR_HEAD_RADIUS, d, e, bust ? PART_BUST : PART_HEAD);
}

/* Del pelo de arriba a la suela de los zapatos, en radios de cabeza */
#define BODY_TOP_R    1.05f
#define BODY_BOTTOM_R 2.48f

void mii_draw_body(const Avatar* a, float cx, float bottom, float height, float d, Expression e)
{
	float s = height / ((BODY_TOP_R + BODY_BOTTOM_R) * CHR_HEAD_RADIUS);
	draw_character(a, cx, bottom - BODY_BOTTOM_R * CHR_HEAD_RADIUS * s, s, d, e, PART_BODY);
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

static void avatar_from_console(Avatar* a, const MiiData* mii, const char* name)
{
	MiiParts parts;
	parts_from_console(&parts, mii);
	avatar_from_mii(a, &parts, name);
}

/* Selector de Miis del sistema (la API oficial de Nintendo para elegir un
 * Mii): muestra los Miis de la consola y, si guests, los Miis invitados. */
static bool pick_mii(Avatar* out, const char* title, bool guests)
{
	MiiSelectorConf conf;
	MiiSelectorReturn ret;
	miiSelectorInit(&conf);
	miiSelectorSetTitle(&conf, title);
	miiSelectorSetOptions(&conf, MIISELECTOR_CANCEL | (guests ? MIISELECTOR_GUESTS : 0));
	if (guests)
		miiSelectorWhitelistGuestMii(&conf, MIISELECTOR_GUESTMII_SLOTS);
	miiSelectorLaunch(&conf, &ret);
	if (ret.no_mii_selected || !miiSelectorChecksumIsValid(&ret))
		return false;
	char name[AVATAR_NAME_MAX];
	miiSelectorReturnGetName(&ret, name, sizeof name);
	avatar_from_console(out, &ret.mii, name);
	return true;
}

bool profile_pick_console_mii(void)
{
	PlayerProfile* p = profile_get();
	Avatar picked;
	if (!pick_mii(&picked, "Elige tu Mii", true))
		return false;
	p->avatar = picked;
	p->from_console = 1;
	p->created = 1;
	profile_store();
	rivals_refresh();
	return true;
}

/* ---------------------------------------------------------------------------
 * Rivales: los Miis de la consola
 * ------------------------------------------------------------------------- */

#define RIVALS_PATH  PROFILE_DIR "/rivales.dat"
#define RIVALS_MAGIC 0x52495631u /* "RIV1" */
#define CFL_DB_MIIS  100
#define CFL_DB_FIRST 0x8 /* los Miis empiezan tras la cabecera "CFOG" */

_Static_assert(sizeof(MiiData) == 0x5C, "MiiData debe ocupar 0x5C bytes como en CFL_DB.dat");

typedef struct {
	uint32_t magic;
	uint8_t manual[CPU_COUNT];
	Avatar seat[CPU_COUNT];
} RivalsFile;

static Avatar s_pool[CFL_DB_MIIS];
static int s_pool_count = -1; /* -1: no se ha podido leer la base de datos */

/* Lee la base de datos de Miis de la consola (CFL_DB.dat), que está en los
 * datos compartidos del sistema 0xF000000B junto a las Monedas de juego. */
static int read_console_miis(Avatar* out, int max)
{
	static const u32 extdata[3] = { MEDIATYPE_NAND, 0xF000000B, 0x00048000 };
	FS_Archive archive;
	FS_Path apath = { PATH_BINARY, sizeof extdata, extdata };
	if (R_FAILED(FSUSER_OpenArchive(&archive, ARCHIVE_SHARED_EXTDATA, apath)))
		return -1;
	Handle file;
	int count = -1;
	if (R_SUCCEEDED(FSUSER_OpenFile(&file, archive, fsMakePath(PATH_ASCII, "/CFL_DB.dat"), FS_OPEN_READ, 0))) {
		const u32 size = CFL_DB_FIRST + CFL_DB_MIIS * sizeof(MiiData);
		u8* buf = malloc(size);
		u32 got = 0;
		if (buf && R_SUCCEEDED(FSFILE_Read(file, &got, 0, buf, size)) && got == size) {
			count = 0;
			for (int i = 0; i < CFL_DB_MIIS && count < max; i++) {
				MiiData mii;
				memcpy(&mii, buf + CFL_DB_FIRST + i * sizeof(MiiData), sizeof mii);
				/* Huecos vacíos: sin identificador o sin nombre */
				if (mii.version != 3 || mii.mii_id == 0 || mii.mii_name[0] == 0)
					continue;
				char name[AVATAR_NAME_MAX];
				u16 name16[11] = { 0 };
				memcpy(name16, mii.mii_name, sizeof mii.mii_name);
				ssize_t n = utf16_to_utf8((uint8_t*)name, name16, sizeof name - 1);
				name[n < 0 ? 0 : n] = '\0';
				avatar_from_console(&out[count++], &mii, name);
			}
		}
		free(buf);
		FSFILE_Close(file);
	}
	FSUSER_CloseArchive(archive);
	return count;
}

static void rivals_store(void)
{
	RivalsFile f;
	memset(&f, 0, sizeof f);
	f.magic = RIVALS_MAGIC;
	for (int i = 0; i < CPU_COUNT; i++)
		if (cpu_is_manual((CpuId)i)) {
			f.manual[i] = 1;
			f.seat[i] = *cpu_avatar((CpuId)i);
		}
	mkdir("sdmc:/3ds", 0777);
	mkdir(PROFILE_DIR, 0777);
	FILE* out = fopen(RIVALS_PATH, "wb");
	if (out) {
		fwrite(&f, 1, sizeof f, out);
		fclose(out);
	}
}

void rivals_refresh(void)
{
	const PlayerProfile* p = profile_get();
	cpu_assign_pool(s_pool, s_pool_count > 0 ? s_pool_count : 0, p->avatar.official ? &p->avatar : NULL);
}

void rivals_init(void)
{
	RivalsFile f;
	FILE* in = fopen(RIVALS_PATH, "rb");
	if (in) {
		if (fread(&f, 1, sizeof f, in) == sizeof f && f.magic == RIVALS_MAGIC)
			for (int i = 0; i < CPU_COUNT; i++)
				if (f.manual[i])
					cpu_set_manual((CpuId)i, &f.seat[i]);
		fclose(in);
	}
	s_pool_count = read_console_miis(s_pool, CFL_DB_MIIS);
	rivals_refresh();
}

int rivals_console_count(void)
{
	return s_pool_count;
}

bool rivals_pick(CpuId id)
{
	char title[64];
	snprintf(title, sizeof title, "Rival: %s", cpu_role(id));
	Avatar picked;
	if (!pick_mii(&picked, title, true))
		return false;
	cpu_set_manual(id, &picked);
	rivals_store();
	rivals_refresh();
	return true;
}

void rivals_reset(void)
{
	for (int i = 0; i < CPU_COUNT; i++)
		cpu_set_manual((CpuId)i, NULL);
	remove(RIVALS_PATH);
	rivals_refresh();
}

