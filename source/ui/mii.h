#ifndef UI_MII_H
#define UI_MII_H

#include <stdbool.h>

#include "core/avatar.h"
#include "core/profile.h"

/* Dibujo de los Miis con su cara oficial (renderizada por Mii Studio de
 * Nintendo y guardada en la SD); mientras no está disponible se ve una
 * silueta. También el perfil del jugador y sus rivales. */

typedef enum {
	EXPR_NORMAL,
	EXPR_HAPPY,
	EXPR_SAD,
	EXPR_SURPRISED,
	EXPR_FRUSTRATED,  /* seña de reyes: apretar los labios */
	EXPR_SMILE_OPEN,  /* seña de ases: sonreír con la boca abierta */
	EXPR_ANGER,       /* seña de medias: fruncir el ceño */
	EXPR_BROWS_UP,    /* seña de duples: levantar las cejas */
	EXPR_WINK,        /* seña de 31: guiñar un ojo */
	EXPR_EYES_CLOSED, /* seña de ciego (y parpadeo) */
	EXPR_COUNT
} Expression;

/* Dibuja el Mii con la cabeza centrada en (cx, cy) y radio size. Con bust se
 * dibujan también los hombros con la camiseta del color favorito. Si el
 * personaje es un Mii de verdad se usa su cara oficial cuando ya está
 * descargada. */
void mii_draw(const Avatar* a, float cx, float cy, float size, float depth, Expression e, bool bust);

/* Estado de ánimo de un personaje: una expresión que dura un tiempo y un
 * parpadeo de vez en cuando para que parezca vivo. */
typedef struct {
	Expression expr;
	float timer;
	float blink;
} Mood;

void mood_set(Mood* m, Expression e, float seconds);
void mood_update(Mood* m, float dt);
Expression mood_expr(const Mood* m);

/* Perfil del jugador */
PlayerProfile* profile_get(void);
void profile_store(void);
/* Guarda una partida (o mano) jugada en las estadísticas. */
void profile_record_game(GameId game, bool won);
/* Abre el selector de Miis de la consola. Devuelve true si se ha elegido uno. */
bool profile_pick_console_mii(void);
/* Rivales: cada puesto de la CPU lo ocupa un Mii de la consola (leídos de
 * su base de datos de Miis) o el que el jugador elija con el selector de
 * Miis del sistema, que también ofrece los Miis invitados. */
void rivals_init(void);
/* Vuelve a repartir los Miis de la consola (por ejemplo, al cambiar tu Mii). */
void rivals_refresh(void);
/* Miis encontrados en la consola, o -1 si no se ha podido leer. */
int rivals_console_count(void);
/* Abre el selector de Miis para el puesto id. */
bool rivals_pick(CpuId id);
/* Olvida las elecciones a mano y vuelve a los Miis de la consola. */
void rivals_reset(void);

/* Expresión equivalente del renderizador de Mii Studio. */
const char* mii_studio_expression(Expression e);

#endif
