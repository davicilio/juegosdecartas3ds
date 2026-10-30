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
	GAME_UNO,
	GAME_RUMMIKUB,
	GAME_MONA,
	GAME_CUCU,
	GAME_TREINTAYUNA,
	GAME_MENTIROSO,
	GAME_CULO,
	GAME_POKER,
	GAME_BLACKJACK,
	GAME_CORAZONES,
	GAME_POCHA,
	GAME_CONECTA4,
	GAME_DAMAS,
	GAME_DOMINO,
	GAME_AS23,
	GAME_PUMBA,
	GAME_JULEPE,
	GAME_TUTE_SUBASTADO,
	GAME_TRUCO,
	GAME_BUTIFARRA,
	GAME_REVERSI,
	GAME_OCA,
	GAME_BURRO,
	GAME_MIERDA,
	GAME_TRUC,
	GAME_TRESILLO,
	GAME_CUATRILLO,
	GAME_BINGO,
	GAME_TRESENRAYA,
	GAME_FLOTA,
	GAME_PARCHIS,
	GAME_FREECELL,
	GAME_TOMA6,
	GAME_MAHJONG,
	GAME_OMAHA,
	GAME_DRAWPOKER,
	GAME_VIDEOPOKER,
	GAME_NEGOCIO,
	GAME_COUNT
} GameId;

/* Huecos de estadísticas en el fichero: sobran para añadir juegos sin
 * cambiar el formato. Las versiones 1 y 2 tenían 6, y la 3, 32. */
#define PROFILE_GAME_SLOTS 64
#define PROFILE_LEGACY_GAMES 6
#define PROFILE_V3_SLOTS 32
_Static_assert(GAME_COUNT <= PROFILE_GAME_SLOTS, "faltan huecos de estadísticas en el perfil");

#define PROFILE_MAGIC   0x4D494931u /* "MII1" */
#define PROFILE_VERSION 5
/* Fichas con las que empieza un perfil nuevo (o uno de antes de la versión 5) */
#define PROFILE_START_CHIPS 2000

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
	/* Monedero: las fichas del jugador, comunes a todos los juegos con apuestas */
	int32_t chips;
	uint32_t reserved2[3];
} PlayerProfile;

void profile_init(PlayerProfile* p);
bool profile_load(PlayerProfile* p, const char* path);
bool profile_save(const PlayerProfile* p, const char* path);
void profile_record(PlayerProfile* p, GameId game, bool won);
/* Monedero. profile_take_chips saca hasta want fichas (las que haya) y
 * devuelve cuántas ha sacado. */
int profile_take_chips(PlayerProfile* p, int want);
void profile_give_chips(PlayerProfile* p, int n);
const char* game_name(GameId game);
/* "partidas" o "manos" según el juego */
const char* game_unit(GameId game);

#endif
