#ifndef UI_WALLET_H
#define UI_WALLET_H

#include <stdbool.h>

/* Monedero de fichas, común a todos los juegos con apuestas y guardado en
 * el perfil. Al empezar una partida se sacan fichas del monedero y al
 * acabarla (o al salir a medias) se devuelven las que queden. */
int wallet_chips(void);
/* Saca hasta want fichas (las que haya) y devuelve cuántas. */
int wallet_take(int want);
void wallet_give(int n);

/* Monedas de juego de la consola (las que se ganan andando con la 3DS).
 * Devuelve -1 si no se pueden leer. */
int playcoins_get(void);
/* Gasta n monedas de juego. Devuelve false si no hay tantas o no se pueden
 * escribir. */
bool playcoins_spend(int n);

/* Fichas que da cada moneda de juego */
#define CHIPS_PER_COIN 100

#endif
