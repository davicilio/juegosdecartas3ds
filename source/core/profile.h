#ifndef CORE_PROFILE_H
#define CORE_PROFILE_H

#include <stdbool.h>
#include <stdint.h>

#include "core/avatar.h"

/* Perfil del jugador: su Mii y sus estadísticas, guardados en la tarjeta SD. */

typedef enum {
	GAME_SIETEYMEDIA,
	GAME_CINQUILLO,
	GAME_BRISCA,
	GAME_ESCOBA,
	GAME_MUS,
	GAME_CHINCHON,
	GAME_COUNT
} GameId;

#define PROFILE_MAGIC   0x4D494931u /* "MII1" */
#define PROFILE_VERSION 2

typedef struct {
	uint32_t magic;
	uint16_t version;
	uint8_t created;      /* el jugador ya ha creado su perfil */
	uint8_t from_console; /* el aspecto viene de un Mii de la consola */
	uint8_t online_miis;  /* descargar las caras oficiales de Mii Studio */
	uint8_t reserved[3];
	Avatar avatar;
	uint32_t played[GAME_COUNT];
	uint32_t won[GAME_COUNT];
} PlayerProfile;

void profile_init(PlayerProfile* p);
bool profile_load(PlayerProfile* p, const char* path);
bool profile_save(const PlayerProfile* p, const char* path);
void profile_record(PlayerProfile* p, GameId game, bool won);
const char* game_name(GameId game);
/* "partidas" o "manos" según el juego */
const char* game_unit(GameId game);

#endif
