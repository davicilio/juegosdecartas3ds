#include "ui/mii.h"

#include <3ds.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "ui/gfx.h"
#include "ui/miiface.h"

/* ---------------------------------------------------------------------------
 * Dibujo
 * ------------------------------------------------------------------------- */

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
	const C2D_Image* img = miiface_get(&a->mii, mii_studio_expression(e), true);
	if (!img && e != EXPR_NORMAL) {
		/* Mientras llega esa expresión, la cara normal (si ya está) */
		img = miiface_get(&a->mii, "normal", true);
	}
	if (!img)
		return false;
	if (bust) {
		u32 fav = gfx_rgb(avatar_favorite_rgb(a->favorite), 255);
		gfx_ellipse(cx, cy + r * 2.0f, r * 1.25f, r * 0.85f, d, fav);
		gfx_rect(cx - r * 0.28f, cy + r * 0.8f, r * 0.56f, r * 0.5f, d, gfx_rgb(avatar_skin_rgb(a->skin), 255));
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
	/* Mientras no está la cara oficial del Mii (se está descargando o no hay
	 * conexión) se ve una silueta, no una imitación del Mii. */
	float pulse = miiface_pending() > 0 ? 0.5f + 0.5f * sinf(gfx_time() * 4.0f) : 0.0f;
	u32 fill = C2D_Color32(150 + (u8)(40 * pulse), 160 + (u8)(40 * pulse), 175 + (u8)(40 * pulse), 230);
	if (bust)
		gfx_ellipse(cx, cy + r * 2.0f, r * 1.25f, r * 0.85f, d, fill);
	gfx_ellipse(cx, cy, r * 0.95f, r * 1.05f, d, fill);
	gfx_ellipse(cx, cy, r * 0.8f, r * 0.9f, d, C2D_Color32(255, 255, 255, 40));
}

void mii_draw_body(const Avatar* a, float cx, float bottom, float height, float d, Expression e)
{
	const C2D_Image* img = NULL;
	if (a->official) {
		img = miiface_get_type(&a->mii, "all_body", mii_studio_expression(e), true);
		if (!img && e != EXPR_NORMAL)
			img = miiface_get_type(&a->mii, "all_body", "normal", true);
	}
	if (img) {
		float scale = height / img->subtex->height;
		C2D_DrawImageAtRotated(*img, cx + gfx_dx(d), bottom - height / 2.0f, 0.5f, 0.0f, NULL, scale, scale);
		return;
	}
	/* Silueta de cuerpo entero mientras se descarga */
	float pulse = miiface_pending() > 0 ? 0.5f + 0.5f * sinf(gfx_time() * 4.0f) : 0.0f;
	u32 fill = C2D_Color32(150 + (u8)(40 * pulse), 160 + (u8)(40 * pulse), 175 + (u8)(40 * pulse), 230);
	float u = height / 10.0f;
	gfx_ellipse(cx, bottom - u * 8.0f, u * 1.6f, u * 1.7f, d, fill);           /* cabeza */
	gfx_rrect(cx - u * 1.5f, bottom - u * 6.1f, u * 3.0f, u * 3.4f, u * 0.8f, d, fill); /* cuerpo */
	gfx_rrect(cx - u * 1.3f, bottom - u * 3.0f, u * 1.1f, u * 3.0f, u * 0.4f, d, fill); /* piernas */
	gfx_rrect(cx + u * 0.2f, bottom - u * 3.0f, u * 1.1f, u * 3.0f, u * 0.4f, d, fill);
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

