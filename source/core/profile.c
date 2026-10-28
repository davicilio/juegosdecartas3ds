#include "core/profile.h"

#include <stdio.h>
#include <string.h>

void profile_init(PlayerProfile* p)
{
	memset(p, 0, sizeof *p);
	p->magic = PROFILE_MAGIC;
	p->version = PROFILE_VERSION;
	p->online_miis = 1;
	avatar_default(&p->avatar);
}

/* Formato de la versión 1 (sin Mii de verdad), para no perder estadísticas */
typedef struct {
	char name[AVATAR_NAME_MAX];
	uint8_t female, skin, hair_style, hair_color, favorite, glasses, facial, long_face;
} AvatarV1;

typedef struct {
	uint32_t magic;
	uint16_t version;
	uint8_t created;
	uint8_t from_console;
	AvatarV1 avatar;
	uint32_t played[PROFILE_LEGACY_GAMES];
	uint32_t won[PROFILE_LEGACY_GAMES];
} PlayerProfileV1;

/* Versión 2: con el Mii, pero solo 6 juegos */
typedef struct {
	uint32_t magic;
	uint16_t version;
	uint8_t created;
	uint8_t from_console;
	uint8_t online_miis;
	uint8_t reserved[3];
	Avatar avatar;
	uint32_t played[PROFILE_LEGACY_GAMES];
	uint32_t won[PROFILE_LEGACY_GAMES];
} PlayerProfileV2;

static bool load_v1(PlayerProfile* p, const uint8_t* buf, size_t n)
{
	PlayerProfileV1 old;
	if (n != sizeof old)
		return false;
	memcpy(&old, buf, sizeof old);
	if (old.magic != PROFILE_MAGIC || old.version != 1)
		return false;
	profile_init(p);
	p->created = old.created;
	memcpy(p->avatar.name, old.avatar.name, sizeof p->avatar.name);
	p->avatar.female = old.avatar.female;
	p->avatar.skin = old.avatar.skin;
	p->avatar.hair_style = old.avatar.hair_style;
	p->avatar.hair_color = old.avatar.hair_color;
	p->avatar.favorite = old.avatar.favorite;
	p->avatar.glasses = old.avatar.glasses;
	p->avatar.facial = old.avatar.facial;
	p->avatar.long_face = old.avatar.long_face;
	avatar_sanitize(&p->avatar);
	memcpy(p->played, old.played, sizeof old.played);
	memcpy(p->won, old.won, sizeof old.won);
	return true;
}

static bool load_v2(PlayerProfile* p, const uint8_t* buf, size_t n)
{
	PlayerProfileV2 old;
	if (n != sizeof old)
		return false;
	memcpy(&old, buf, sizeof old);
	if (old.magic != PROFILE_MAGIC || old.version != 2)
		return false;
	profile_init(p);
	p->created = old.created;
	p->from_console = old.from_console;
	p->online_miis = old.online_miis;
	p->avatar = old.avatar;
	avatar_sanitize(&p->avatar);
	memcpy(p->played, old.played, sizeof old.played);
	memcpy(p->won, old.won, sizeof old.won);
	return true;
}

bool profile_load(PlayerProfile* p, const char* path)
{
	PlayerProfile tmp;
	uint8_t buf[sizeof tmp + 1];
	FILE* f = fopen(path, "rb");
	if (!f)
		return false;
	size_t n = fread(buf, 1, sizeof buf, f);
	fclose(f);
	if (load_v1(p, buf, n) || load_v2(p, buf, n))
		return true;
	if (n != sizeof tmp)
		return false;
	memcpy(&tmp, buf, sizeof tmp);
	if (tmp.magic != PROFILE_MAGIC || tmp.version != PROFILE_VERSION)
		return false;
	avatar_sanitize(&tmp.avatar);
	*p = tmp;
	return true;
}

bool profile_save(const PlayerProfile* p, const char* path)
{
	FILE* f = fopen(path, "wb");
	if (!f)
		return false;
	bool ok = fwrite(p, 1, sizeof *p, f) == sizeof *p;
	return fclose(f) == 0 && ok;
}

void profile_record(PlayerProfile* p, GameId game, bool won)
{
	if (game >= GAME_COUNT)
		return;
	p->played[game]++;
	if (won)
		p->won[game]++;
}

const char* game_name(GameId game)
{
	static const char* names[GAME_COUNT] = { "Siete y media", "Cinquillo", "Brisca", "Escoba", "Mus", "Chinchón",
		"Guiñote", "Solitario", "Pirámide", "Tute", "Tute cabrón", "Uno",
		"Rummikub", "La mona", "Cucú", "Treinta y una", "Mentiroso", "Culo" };
	return game < GAME_COUNT ? names[game] : "?";
}

const char* game_unit(GameId game)
{
	return game == GAME_SIETEYMEDIA ? "manos" : "partidas";
}
