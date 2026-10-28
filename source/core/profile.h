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
	GAME_GUINOTE,
	GAME_SOLITARIO,
	GAME_PIRAMIDE,
	GAME_TUTE,
	GAME_TUTE_CABRON,
	GAME_COUNT
} GameId;

/* Huecos de estadísticas en el fichero: sobran para añadir juegos sin
 * cambiar el formato. Las versiones 1 y 2 tenían 6. */
#define PROFILE_GAME_SLOTS 32
#define PROFILE_LEGACY_GAMES 6
_Static_assert(GAME_COUNT <= PROFILE_GAME_SLOTS, "faltan huecos de estadísticas en el perfil");

#define PROFILE_MAGIC   0x4D494931u /* "MII1" */
#define PROFILE_VERSION 3

typedef struct {
	uint32_t magic;
	uint16_t version;
	uint8_t created;      /* el jugador ya ha creado su perfil */
	uint8_t from_console; /* el aspecto viene de un Mii de la consola */
	uint8_t online_miis;  /* sin uso (antes: descargar caras de Mii Studio) */
	uint8_t reserved[3];
	Avatar avatar;
	uint32_t played[PROFILE_GAME_SLOTS];
	uint32_t won[PROFILE_GAME_SLOTS];
} PlayerProfile;

void profile_init(PlayerProfile* p);
bool profile_load(PlayerProfile* p, const char* path);
bool profile_save(const PlayerProfile* p, const char* path);
void profile_record(PlayerProfile* p, GameId game, bool won);
const char* game_name(GameId game);
/* "partidas" o "manos" según el juego */
const char* game_unit(GameId game);

#endif
