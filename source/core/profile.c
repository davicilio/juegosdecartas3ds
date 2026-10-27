#include "core/profile.h"

#include <stdio.h>
#include <string.h>

void profile_init(PlayerProfile* p)
{
	memset(p, 0, sizeof *p);
	p->magic = PROFILE_MAGIC;
	p->version = PROFILE_VERSION;
	avatar_default(&p->avatar);
}

bool profile_load(PlayerProfile* p, const char* path)
{
	PlayerProfile tmp;
	FILE* f = fopen(path, "rb");
	if (!f)
		return false;
	size_t n = fread(&tmp, 1, sizeof tmp, f);
	fclose(f);
	if (n != sizeof tmp || tmp.magic != PROFILE_MAGIC || tmp.version != PROFILE_VERSION)
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
	static const char* names[GAME_COUNT] = { "Siete y media", "Cinquillo", "Brisca", "Escoba", "Mus", "Chinchón" };
	return game < GAME_COUNT ? names[game] : "?";
}

const char* game_unit(GameId game)
{
	return game == GAME_SIETEYMEDIA ? "manos" : "partidas";
}
