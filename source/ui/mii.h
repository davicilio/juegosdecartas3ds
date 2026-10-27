#ifndef UI_MII_H
#define UI_MII_H

#include <stdbool.h>

#include "core/avatar.h"
#include "core/profile.h"

/* Dibujo de personajes al estilo Mii, sus caras (expresiones y señas) y el
 * perfil del jugador guardado en la SD. */

typedef enum {
	EXPR_NORMAL,
	EXPR_HAPPY,
	EXPR_SAD,
	EXPR_SURPRISED,
	EXPR_BITE_LIP,    /* seña de reyes */
	EXPR_TONGUE,      /* seña de ases */
	EXPR_MOUTH_SIDE,  /* seña de medias */
	EXPR_BROWS_UP,    /* seña de duples */
	EXPR_WINK,        /* seña de 31 */
	EXPR_EYES_CLOSED, /* seña de ciego (y parpadeo) */
	EXPR_COUNT
} Expression;

/* Dibuja el Mii con la cabeza centrada en (cx, cy) y radio size. Con bust se
 * dibujan también los hombros con la camiseta del color favorito. */
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
/* Abre el teclado de la consola para cambiar el nombre. */
bool profile_edit_name(void);

#endif
