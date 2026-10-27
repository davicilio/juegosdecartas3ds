#include "core/avatar.h"

#include <stdio.h>
#include <string.h>

static const uint32_t SKIN[AVATAR_SKINS] = { 0xFFD3AD, 0xFFB67B, 0xDE7942, 0xFFAA8C, 0xAD5129, 0x632C18 };
static const uint32_t HAIR[AVATAR_HAIR_COLORS] = {
	0x1E1A18, 0x402018, 0x5C180A, 0x7C3A14, 0x787880, 0x4E3E10, 0x885818, 0xD0A04A
};
static const uint32_t FAVORITE[AVATAR_FAVORITES] = {
	0xD21E14, 0xFF6E19, 0xFFD820, 0x78D220, 0x007830, 0x0A48B4,
	0x3CAADE, 0xF55A7D, 0x7328AD, 0x483818, 0xE0E0E0, 0x181814
};
static const char* const FAVORITE_NAMES[AVATAR_FAVORITES] = {
	"Rojo", "Naranja", "Amarillo", "Verde claro", "Verde", "Azul",
	"Celeste", "Rosa", "Morado", "Marrón", "Blanco", "Negro"
};
static const char* const HAIR_NAMES[HAIR_STYLES] = {
	"Corto", "Largo", "Moño", "De punta", "Calvo", "Media melena", "Coleta", "Rizado"
};

/* Miis de la CPU. Cada uno es un Mii de verdad, hecho con las piezas del
 * editor de Nintendo; el aspecto simplificado se deduce de esas piezas. */
static void make_cpu(Avatar* a, const char* name, bool female, void (*shape)(MiiParts*))
{
	MiiParts m;
	mii_parts_default(&m, female);
	shape(&m);
	mii_parts_sanitize(&m);
	avatar_from_mii(a, &m, name);
}

static void paco(MiiParts* m)
{
	m->favorite_color = 5;
	m->hair_type = 30;
	m->face_color = 1;
	m->mustache_type = 1;
	m->facial_hair_color = 1;
	m->build = 80;
}

static void lola(MiiParts* m)
{
	m->favorite_color = 0;
	m->hair_color = 0;
	m->face_makeup = 1;
	m->mouth_type = 17;
}

static void manolo(MiiParts* m)
{
	m->favorite_color = 4;
	m->hair_type = 75;
	m->hair_color = 4;
	m->brow_color = 4;
	m->beard_type = 2;
	m->facial_hair_color = 4;
	m->glasses_type = 1;
	m->face_color = 2;
	m->face_type = 3;
	m->face_wrinkles = 1;
	m->height = 50;
	m->build = 100;
}

static void carmen(MiiParts* m)
{
	m->favorite_color = 7;
	m->hair_type = 24;
	m->hair_color = 2;
	m->eye_type = 10;
	m->brow_type = 1;
	m->mouth_type = 9;
	m->face_color = 1;
	m->face_makeup = 2;
}

static void pepe(MiiParts* m)
{
	m->favorite_color = 2;
	m->hair_type = 41;
	m->hair_color = 6;
	m->brow_color = 6;
	m->eye_type = 0;
	m->brow_type = 9;
	m->mouth_type = 6;
	m->face_color = 3;
	m->height = 90;
	m->build = 50;
}

static void lucia(MiiParts* m)
{
	m->favorite_color = 8;
	m->hair_type = 87;
	m->hair_color = 0;
	m->eye_type = 17;
	m->glasses_type = 3;
	m->glasses_color = 3;
	m->face_color = 4;
	m->mouth_type = 12;
}

static void ramon(MiiParts* m)
{
	m->favorite_color = 11;
	m->hair_type = 5;
	m->hair_color = 4;
	m->brow_color = 4;
	m->beard_type = 4;
	m->mustache_type = 2;
	m->facial_hair_color = 4;
	m->glasses_type = 1;
	m->face_wrinkles = 4;
	m->face_type = 6;
	m->eye_type = 21;
	m->brow_type = 12;
	m->nose_type = 5;
	m->mouth_type = 30;
}

const Avatar* cpu_avatar(CpuId id)
{
	static Avatar roster[CPU_COUNT];
	static bool ready;
	if (!ready) {
		make_cpu(&roster[CPU_PACO], "Paco", false, paco);
		make_cpu(&roster[CPU_LOLA], "Lola", true, lola);
		make_cpu(&roster[CPU_MANOLO], "Manolo", false, manolo);
		make_cpu(&roster[CPU_CARMEN], "Carmen", true, carmen);
		make_cpu(&roster[CPU_PEPE], "Pepe", false, pepe);
		make_cpu(&roster[CPU_LUCIA], "Lucía", true, lucia);
		make_cpu(&roster[CPU_RAMON], "Don Ramón", false, ramon);
		ready = true;
	}
	return &roster[id < CPU_COUNT ? id : 0];
}

void avatar_from_mii(Avatar* a, const MiiParts* mii, const char* name)
{
	memset(a, 0, sizeof *a);
	snprintf(a->name, sizeof a->name, "%s", name);
	a->official = 1;
	a->mii = *mii;
	a->female = mii->gender;
	a->skin = mii->face_color;
	a->hair_color = mii->hair_color;
	a->favorite = mii->favorite_color;
	a->glasses = mii->glasses_type != 0;
	a->facial = mii->beard_type ? FACIAL_BEARD : mii->mustache_type ? FACIAL_MUSTACHE : FACIAL_NONE;
	a->long_face = mii->face_type >= 6;
	/* Los 132 peinados del editor se agrupan en los ocho del dibujo simplificado */
	static const uint8_t female_styles[] = { HAIR_LONG, HAIR_BOB, HAIR_PONYTAIL, HAIR_BUN, HAIR_CURLY, HAIR_SHORT };
	static const uint8_t male_styles[] = { HAIR_SHORT, HAIR_SPIKY, HAIR_CURLY, HAIR_BALD, HAIR_SHORT, HAIR_BOB };
	unsigned k = mii->hair_type % 6;
	a->hair_style = a->female ? female_styles[k] : male_styles[k];
	avatar_sanitize(a);
}

void avatar_default(Avatar* a)
{
	memset(a, 0, sizeof *a);
	snprintf(a->name, sizeof a->name, "Jugador");
	a->skin = 0;
	a->hair_style = HAIR_SHORT;
	a->hair_color = 3;
	a->favorite = 6;
}

void avatar_sanitize(Avatar* a)
{
	a->name[AVATAR_NAME_MAX - 1] = '\0';
	if (a->name[0] == '\0')
		snprintf(a->name, sizeof a->name, "Jugador");
	a->female = a->female ? 1 : 0;
	a->skin %= AVATAR_SKINS;
	a->hair_style %= HAIR_STYLES;
	a->hair_color %= AVATAR_HAIR_COLORS;
	a->favorite %= AVATAR_FAVORITES;
	a->glasses = a->glasses ? 1 : 0;
	a->facial %= FACIAL_STYLES;
	a->long_face = a->long_face ? 1 : 0;
	a->official = a->official ? 1 : 0;
	mii_parts_sanitize(&a->mii);
}

uint32_t avatar_skin_rgb(int skin)
{
	return SKIN[(unsigned)skin % AVATAR_SKINS];
}

uint32_t avatar_hair_rgb(int color)
{
	return HAIR[(unsigned)color % AVATAR_HAIR_COLORS];
}

uint32_t avatar_favorite_rgb(int favorite)
{
	return FAVORITE[(unsigned)favorite % AVATAR_FAVORITES];
}

const char* avatar_favorite_name(int favorite)
{
	return FAVORITE_NAMES[(unsigned)favorite % AVATAR_FAVORITES];
}

const char* avatar_hair_style_name(int style)
{
	return HAIR_NAMES[(unsigned)style % HAIR_STYLES];
}
