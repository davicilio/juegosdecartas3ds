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

/*                                   nombre      mujer piel peinado        pelo fav gafas  facial          cara larga */
static const Avatar ROSTER[CPU_COUNT] = {
	[CPU_PACO]   = { "Paco",      0, 1, HAIR_SHORT,    1, 5,  0, FACIAL_MUSTACHE, 0 },
	[CPU_LOLA]   = { "Lola",      1, 0, HAIR_LONG,     0, 0,  0, FACIAL_NONE,     0 },
	[CPU_MANOLO] = { "Manolo",    0, 2, HAIR_BALD,     4, 4,  1, FACIAL_BEARD,    1 },
	[CPU_CARMEN] = { "Carmen",    1, 1, HAIR_BUN,      2, 7,  0, FACIAL_NONE,     0 },
	[CPU_PEPE]   = { "Pepe",      0, 3, HAIR_SPIKY,    6, 2,  0, FACIAL_NONE,     0 },
	[CPU_LUCIA]  = { "Lucía",     1, 4, HAIR_PONYTAIL, 0, 8,  1, FACIAL_NONE,     0 },
	[CPU_RAMON]  = { "Don Ramón", 0, 0, HAIR_BOB,      4, 11, 1, FACIAL_BEARD,    1 },
};

const Avatar* cpu_avatar(CpuId id)
{
	return &ROSTER[id < CPU_COUNT ? id : 0];
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
